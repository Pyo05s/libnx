// 파일: rtp_receiver.cpp
// 생성일: 2026-02-23
// 설명: RTP 수신기 구현

#include "rtp_receiver.h"
#include "rtp_packet.h"

#include <spdlog/spdlog.h>

namespace nx::rtp {

RtpReceiver::RtpReceiver(AsioContext& ioc, TransportMode mode)
    : m_ioc(ioc)
    , m_mode(mode)
    , m_frame_pool(std::make_shared<nx::media::MediaFrameBufferPool>())
{}

RtpReceiver::~RtpReceiver()
{
  stop();
}

nx::awaitable<std::error_code>
RtpReceiver::bind_udp(uint16_t local_rtp_port, uint16_t local_rtcp_port)
{
  if (m_mode != TransportMode::kUdp) {
    co_return make_error_code(RtpErrc::bind_failed);
  }

  try {
    m_rtp_socket = std::make_unique<boost::asio::ip::udp::socket>(m_ioc);
    m_rtp_socket->open(boost::asio::ip::udp::v4());
    m_rtp_socket->bind(
      boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), local_rtp_port));

    m_rtcp_socket = std::make_unique<boost::asio::ip::udp::socket>(m_ioc);
    m_rtcp_socket->open(boost::asio::ip::udp::v4());
    m_rtcp_socket->bind(
      boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), local_rtcp_port));

    spdlog::debug("RTP UDP 바인딩: RTP={}, RTCP={}", local_rtp_port, local_rtcp_port);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    spdlog::error("RTP UDP 바인딩 실패: {}", e.what());
    co_return make_error_code(RtpErrc::bind_failed);
  }
}

void
RtpReceiver::feed_data(uint8_t /*channel*/, std::span<const uint8_t> data)
{
  // TCP Interleaved 모드: 외부에서 RTP 데이터 주입
  handle_rtp_packet(data);
}

void
RtpReceiver::set_depacketizer(std::unique_ptr<RtpDepacketizer> depacketizer)
{
  m_depacketizer = std::move(depacketizer);
}

void
RtpReceiver::set_frame_callback(FrameCallback callback)
{
  m_frame_callback = std::move(callback);
}

void
RtpReceiver::set_expected_payload_type(uint8_t pt)
{
  m_expected_pt = pt;
  m_pt_initialized = true;
}

void
RtpReceiver::set_payload_type_mismatch_callback(PayloadTypeMismatchCallback callback)
{
  m_pt_mismatch_callback = std::move(callback);
}

nx::awaitable<void>
RtpReceiver::start_receiving()
{
  if (m_mode != TransportMode::kUdp || !m_rtp_socket) {
    co_return;
  }

  m_running = true;
  std::array<uint8_t, 65536> buffer;

  spdlog::debug("RTP UDP 수신 시작");

  while (m_running) {
    try {
      boost::asio::ip::udp::endpoint sender;
      auto bytes = co_await m_rtp_socket->async_receive_from(
        boost::asio::buffer(buffer),
        sender,
        boost::asio::use_awaitable);

      if (bytes > 0) {
        handle_rtp_packet(std::span<const uint8_t>(buffer.data(), bytes));
      }
    }
    catch (const boost::system::system_error& e) {
      if (e.code() == boost::asio::error::operation_aborted) {
        break;
      }
      spdlog::error("RTP 수신 오류: {}", e.what());
    }
  }

  spdlog::debug("RTP UDP 수신 종료");
}

void
RtpReceiver::stop()
{
  m_running = false;

  if (m_rtp_socket && m_rtp_socket->is_open()) {
    boost::system::error_code ec;
    m_rtp_socket->cancel(ec);
    m_rtp_socket->close(ec);
  }

  if (m_rtcp_socket && m_rtcp_socket->is_open()) {
    boost::system::error_code ec;
    m_rtcp_socket->cancel(ec);
    m_rtcp_socket->close(ec);
  }
}

RtpStatistics
RtpReceiver::get_statistics() const noexcept
{
  return m_statistics;
}

void
RtpReceiver::handle_rtp_packet(std::span<const uint8_t> data)
{
  std::span<const uint8_t> payload;
  RtpHeaderView header;

  auto ec = RtpPacket::parse_header_and_payload(data, header, payload);
  if (ec) {
    return;
  }

  // PT 불일치 감지 (SETUP 시 협상된 코덱과 다른 데이터 수신)
  if (m_pt_initialized && header.payload_type != m_expected_pt) {
    if (header.timestamp != m_last_mismatch_timestamp) {
      m_last_mismatch_timestamp = header.timestamp;
      ++m_pt_mismatch_frames;
    }
    if (
      !m_pt_mismatch_reported && m_pt_mismatch_frames >= kPtMismatchThreshold
      && m_pt_mismatch_callback) {
      m_pt_mismatch_reported = true;
      spdlog::warn(
        "RTP Payload Type 불일치 감지: expected={}, actual={}",
        m_expected_pt,
        header.payload_type);
      m_pt_mismatch_callback(m_expected_pt, header.payload_type);
    }
    return;
  }

  // 통계 업데이트
  m_statistics.packets_received++;
  m_statistics.bytes_received += data.size();

  // 시퀀스 번호 추적
  if (!m_seq_initialized) {
    m_expected_seq = header.sequence_number;
    m_seq_initialized = true;
  }
  else {
    int16_t diff = static_cast<int16_t>(header.sequence_number - m_expected_seq);
    if (diff > 0) {
      m_statistics.packets_lost += diff;
    }
    else if (diff < 0) {
      m_statistics.packets_out_of_order++;
    }
  }
  m_expected_seq = header.sequence_number + 1;

  // 디패킷타이징
  if (m_frame_callback) {
    if (m_depacketizer) {
      auto buf = m_frame_pool->acquire();
      bool keyframe = false;

      if (m_depacketizer->process_packet(header, payload, *buf, keyframe)) {
        m_frame_callback(
          std::move(buf),
          static_cast<uint64_t>(header.timestamp),
          keyframe);
      }
    }
    else {
      // 디패킷타이저 미설정: RTP payload를 그대로 프레임으로 전달
      // G.711 등 단순 오디오 코덱은 RTP 패킷 하나가 완전한 프레임
      if (!m_passthrough_buffer) {
        m_passthrough_buffer = m_frame_pool->acquire();
      }
      auto& buf = *m_passthrough_buffer;
      buf.assign(payload.begin(), payload.end());
      m_frame_callback(
        m_passthrough_buffer,
        static_cast<uint64_t>(header.timestamp),
        false);
    }
  }
}

} // namespace nx::rtp
