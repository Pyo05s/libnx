// 파일: rtsp_server_session.cpp
// 생성일: 2026-02-26
// 설명: RTSP 서버 클라이언트 세션 구현

#include "rtsp_server_session.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <boost/asio/write.hpp>
#include <boost/asio/read_until.hpp>
#include <boost/asio/buffer.hpp>
#include <boost/asio/streambuf.hpp>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

#include <algorithm>
#include <random>
#include <sstream>

namespace nx::net {

// ============================================================================
// TcpInterleavedTransport
// ============================================================================

TcpInterleavedTransport::TcpInterleavedTransport(
  std::shared_ptr<TcpWriteSerializer> serializer,
  std::shared_ptr<boost::asio::ip::tcp::socket> socket,
  uint8_t rtp_channel,
  uint8_t rtcp_channel)
    : m_serializer(std::move(serializer))
    , m_socket(std::move(socket))
    , m_rtp_channel(rtp_channel)
    , m_rtcp_channel(rtcp_channel)
{}

void
TcpInterleavedTransport::send_rtp(std::span<const uint8_t> packet)
{
  send_interleaved(m_rtp_channel, packet);
}

void
TcpInterleavedTransport::send_rtp_batch(std::span<const std::vector<uint8_t>> packets)
{
  if (!is_active() || packets.empty()) {
    return;
  }

  // 전체 크기 사전 계산 — 힙 재할당 0회
  size_t total = 0;
  for (const auto& pkt : packets) {
    total += 4 + pkt.size(); // 인터리브 헤더(4) + 페이로드
  }

  // 단일 연속 버퍼에 모든 인터리브 프레임 병합: async_write 1회 → IOCP 완료 이벤트
  // 1회
  std::vector<uint8_t> merged;
  merged.reserve(total);

  const uint8_t channel = m_rtp_channel;
  for (const auto& pkt : packets) {
    if (pkt.empty()) {
      continue;
    }
    auto length = static_cast<uint16_t>(pkt.size());
    merged.push_back('$');
    merged.push_back(channel);
    merged.push_back(static_cast<uint8_t>(length >> 8));
    merged.push_back(static_cast<uint8_t>(length & 0xFF));
    merged.insert(merged.end(), pkt.begin(), pkt.end());
  }

  if (!merged.empty()) {
    m_serializer->submit(std::move(merged));
  }
}

void
TcpInterleavedTransport::send_rtp_frame(const nx::rtp::SharedRtpFrame& frame_buffer)
{
  if (!is_active() || !frame_buffer || frame_buffer->packet_count() == 0) {
    return;
  }

  // WriteEntry 구성: 패킷당 4바이트 TCP 인터리브 헤더 + SharedRtpFrame 공유
  WriteEntry entry;
  entry.frame_ref = frame_buffer;

  auto count = frame_buffer->packet_count();
  entry.headers.reserve(count);

  const uint8_t channel = m_rtp_channel;
  for (size_t i = 0; i < count; ++i) {
    auto pkt = frame_buffer->packet(i);
    auto length = static_cast<uint16_t>(pkt.size());
    entry.headers.push_back(
      {'$',
       channel,
       static_cast<uint8_t>(length >> 8),
       static_cast<uint8_t>(length & 0xFF)});
  }

  m_serializer->submit_frame(std::move(entry));
}

void
TcpInterleavedTransport::send_rtcp(std::span<const uint8_t> packet)
{
  send_interleaved(m_rtcp_channel, packet);
}

bool
TcpInterleavedTransport::is_active() const
{
  return m_serializer && m_serializer->is_active();
}

std::size_t
TcpInterleavedTransport::queued_count() const
{
  return m_serializer ? m_serializer->queued_count() : 0;
}

std::size_t
TcpInterleavedTransport::overflow_count() const
{
  return m_serializer ? m_serializer->overflow_count() : 0;
}

const TcpWriteSerializer*
TcpInterleavedTransport::serializer_address() const
{
  return m_serializer.get();
}

void
TcpInterleavedTransport::close()
{
  // 직렬화기 종료 (대기 큐 폐기, 추가 쓰기 차단)
  if (m_serializer) {
    m_serializer->close();
  }

  // 소켓 닫기 → read_loop 에러 → RtspServerSession::close() 자동 호출
  if (m_socket && m_socket->is_open()) {
    boost::system::error_code ec;
    m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket->close(ec);
  }
}

void
TcpInterleavedTransport::send_interleaved(
  uint8_t channel, std::span<const uint8_t> data)
{
  if (!is_active() || data.empty()) {
    return;
  }

  // '$' + channel(1byte) + length(2bytes big-endian) + data
  auto length = static_cast<uint16_t>(data.size());
  std::vector<uint8_t> frame;
  frame.reserve(4 + data.size());
  frame.push_back('$');
  frame.push_back(channel);
  frame.push_back(static_cast<uint8_t>(length >> 8));
  frame.push_back(static_cast<uint8_t>(length & 0xFF));
  frame.insert(frame.end(), data.begin(), data.end());

  // 직렬화기에 제출 — 즉시 리턴 (non-blocking)
  m_serializer->submit(std::move(frame));
}

// ============================================================================
// RtspServerSession 생성자/소멸자
// ============================================================================

RtspServerSession::RtspServerSession(
  boost::asio::ip::tcp::socket socket, std::shared_ptr<IRtspSessionRegistry> registry)
    : m_socket(std::make_shared<boost::asio::ip::tcp::socket>(std::move(socket)))
    , m_registry(std::move(registry))
    , m_write_serializer(std::make_shared<TcpWriteSerializer>(m_socket))
{
  boost::system::error_code ec;

  // TIME_WAIT 상태 소켓 재사용 허용
  m_socket->set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    spdlog::warn("[RtspServerSession] SO_REUSEADDR 설정 실패: {}", ec.message());
  }

  // RTP over TCP Interleaved: 작은 패킷(~1400B)을 빈번히 전송하므로
  // Nagle 비활성화 필수 (Nagle + Delayed ACK 조합 시 커널 송신 버퍼 정체 발생)
  m_socket->set_option(boost::asio::ip::tcp::no_delay(true), ec);
  if (ec) {
    spdlog::warn("[RtspServerSession] TCP_NODELAY 설정 실패: {}", ec.message());
  }
}

RtspServerSession::~RtspServerSession()
{
  close();
}

std::string
RtspServerSession::client_address() const
{
  if (!m_socket || !m_socket->is_open()) {
    return "disconnected";
  }
  boost::system::error_code ec;
  auto ep = m_socket->remote_endpoint(ec);
  if (ec) {
    return "unknown";
  }
  return fmt::format("{}:{}", ep.address().to_string(), ep.port());
}

// ============================================================================
// 세션 시작/종료
// ============================================================================

void
RtspServerSession::start()
{
  m_running = true;
  spdlog::info("[RtspServerSession] 클라이언트 연결: {}", client_address());

  auto self = shared_from_this();
  boost::asio::co_spawn(
    m_socket->get_executor(),
    [self]() -> nx::awaitable<void> { co_await self->read_loop(); },
    boost::asio::detached);
}

void
RtspServerSession::close()
{
  if (!m_running) {
    return;
  }
  m_running = false;

  // 미디어 세션에 teardown 알림
  if (m_media_session) {
    for (auto& setup : m_track_setups) {
      if (setup.rtp_transport) {
        m_media_session->on_teardown(
          setup.track_index,
          setup.rtp_transport,
          m_session_id);
        setup.rtp_transport.reset();
      }
    }
  }

  // 직렬화기 종료 (대기 큐 폐기)
  if (m_write_serializer) {
    m_write_serializer->close();
  }

  // 소켓 닫기
  if (m_socket && m_socket->is_open()) {
    boost::system::error_code ec;
    m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket->close(ec);
  }

  spdlog::info("[RtspServerSession] 클라이언트 연결 종료");
}

// ============================================================================
// RTSP 읽기 루프
// ============================================================================

nx::awaitable<void>
RtspServerSession::read_loop()
{
  try {
    while (m_running && m_socket->is_open()) {
      // 데이터 수신
      char buf[4096];
      auto bytes = co_await m_socket->async_read_some(
        boost::asio::buffer(buf),
        boost::asio::use_awaitable);

      m_recv_buffer.append(buf, bytes);

      // 완전한 RTSP 메시지 파싱 시도
      while (true) {
        auto end_pos = m_recv_buffer.find("\r\n\r\n");
        if (end_pos == std::string::npos) {
          break;
        }

        // Content-Length 확인
        auto msg_header = std::string_view(m_recv_buffer).substr(0, end_pos + 4);
        size_t content_length = 0;
        auto cl_pos = msg_header.find("Content-Length:");
        if (cl_pos == std::string::npos) {
          cl_pos = msg_header.find("content-length:");
        }
        if (cl_pos != std::string::npos) {
          auto val_start = cl_pos + 15;
          while (val_start < msg_header.size() && msg_header[val_start] == ' ') {
            ++val_start;
          }
          auto val_end = msg_header.find("\r\n", val_start);
          if (val_end != std::string::npos) {
            content_length = std::stoull(
              std::string(msg_header.substr(val_start, val_end - val_start)));
          }
        }

        size_t total_size = end_pos + 4 + content_length;
        if (m_recv_buffer.size() < total_size) {
          break; // 본문 미수신
        }

        // 메시지 파싱 및 처리
        auto msg = std::string_view(m_recv_buffer).substr(0, total_size);
        auto req = parse_request(msg);

        if (req) {
          // 메서드별 핸들러 호출
          if (req->method == "OPTIONS") {
            co_await handle_options(*req);
          }
          else if (req->method == "DESCRIBE") {
            co_await handle_describe(*req);
          }
          else if (req->method == "SETUP") {
            co_await handle_setup(*req);
          }
          else if (req->method == "PLAY") {
            co_await handle_play(*req);
          }
          else if (req->method == "PAUSE") {
            co_await handle_pause(*req);
          }
          else if (req->method == "TEARDOWN") {
            co_await handle_teardown(*req);
          }
          else {
            co_await send_response(405, "Method Not Allowed", req->cseq);
          }
        }

        m_recv_buffer.erase(0, total_size);
      }
    }
  }
  catch (const boost::system::system_error& e) {
    if (
      e.code() != boost::asio::error::eof
      && e.code() != boost::asio::error::operation_aborted) {
      spdlog::warn("[RtspServerSession] 읽기 오류: {}", e.what());
    }
  }

  close();
}

// ============================================================================
// 요청 파서
// ============================================================================

std::optional<RtspServerSession::ParsedRequest>
RtspServerSession::parse_request(std::string_view data)
{
  ParsedRequest req;

  // 요청 라인 파싱
  auto first_line_end = data.find("\r\n");
  if (first_line_end == std::string_view::npos) {
    return std::nullopt;
  }

  auto first_line = data.substr(0, first_line_end);
  auto space1 = first_line.find(' ');
  if (space1 == std::string_view::npos) {
    return std::nullopt;
  }
  auto space2 = first_line.find(' ', space1 + 1);
  if (space2 == std::string_view::npos) {
    return std::nullopt;
  }

  req.method = std::string(first_line.substr(0, space1));
  req.uri = std::string(first_line.substr(space1 + 1, space2 - space1 - 1));
  req.version = std::string(first_line.substr(space2 + 1));

  // 헤더 파싱
  auto headers_start = first_line_end + 2;
  auto header_end = data.find("\r\n\r\n", headers_start);
  if (header_end == std::string_view::npos) {
    header_end = data.size();
  }

  auto headers_section = data.substr(headers_start, header_end - headers_start);
  size_t pos = 0;
  while (pos < headers_section.size()) {
    auto line_end = headers_section.find("\r\n", pos);
    if (line_end == std::string_view::npos) {
      line_end = headers_section.size();
    }

    auto line = headers_section.substr(pos, line_end - pos);
    auto colon = line.find(':');
    if (colon != std::string_view::npos) {
      auto key = std::string(line.substr(0, colon));
      auto val_start = colon + 1;
      while (val_start < line.size() && line[val_start] == ' ') {
        ++val_start;
      }
      auto val = std::string(line.substr(val_start));
      req.headers[key] = val;
    }

    pos = line_end + 2;
  }

  // CSeq 추출
  auto cseq_it = req.headers.find("CSeq");
  if (cseq_it == req.headers.end()) {
    cseq_it = req.headers.find("cseq");
  }
  if (cseq_it != req.headers.end()) {
    try {
      req.cseq = static_cast<uint32_t>(std::stoul(cseq_it->second));
    }
    catch (...) {
      req.cseq = 0;
    }
  }

  // 본문 추출
  if (header_end + 4 < data.size()) {
    req.body = std::string(data.substr(header_end + 4));
  }

  return req;
}

// ============================================================================
// OPTIONS
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_options(const ParsedRequest& req)
{
  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Public", "OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN"}
  });
}

// ============================================================================
// DESCRIBE
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_describe(const ParsedRequest& req)
{
  auto path = extract_path(req.uri);

  // 미디어 세션 검색
  m_media_session = m_registry->find_session(path);
  if (!m_media_session) {
    spdlog::warn("[RtspServerSession] 미디어 세션 미존재: path={}", path);
    co_await send_response(404, "Not Found", req.cseq);
    co_return;
  }

  auto sdp_text = m_media_session->sdp();

  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Content-Type", "application/sdp"},
      {"Content-Base",     req.uri + "/"}
  },
    sdp_text);

  m_state = RtspSessionState::kDescribed;
}

// ============================================================================
// SETUP
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_setup(const ParsedRequest& req)
{
  if (!m_media_session) {
    co_await send_response(455, "Method Not Valid in This State", req.cseq);
    co_return;
  }

  // 세션 ID 생성 (첫 SETUP 시)
  if (m_session_id.empty()) {
    m_session_id = generate_session_id();
    // 트랙 셋업 목록 초기화
    m_track_setups.resize(m_media_session->track_count());
    for (size_t i = 0; i < m_track_setups.size(); ++i) {
      m_track_setups[i].track_index = i;
    }
  }

  // URI에서 트랙 인덱스 추출 (예: .../track0, .../trackID=0)
  size_t track_index = 0;
  auto uri = req.uri;
  auto track_pos = uri.find("track");
  if (track_pos != std::string::npos) {
    auto num_start = track_pos + 5;
    // "trackID=" 형식 처리
    if (uri.size() > num_start && uri.substr(num_start, 3) == "ID=") {
      num_start += 3;
    }
    try {
      track_index = std::stoull(uri.substr(num_start));
    }
    catch (...) {
      track_index = 0;
    }
  }

  if (track_index >= m_track_setups.size()) {
    co_await send_response(404, "Not Found", req.cseq);
    co_return;
  }

  // Transport 헤더 파싱
  auto transport_it = req.headers.find("Transport");
  if (transport_it == req.headers.end()) {
    co_await send_response(461, "Unsupported Transport", req.cseq);
    co_return;
  }

  // TCP Interleaved 지원
  auto& transport_str = transport_it->second;
  bool is_tcp = transport_str.find("TCP") != std::string::npos
                || transport_str.find("interleaved") != std::string::npos;

  if (!is_tcp) {
    // UDP는 현재 미지원 → TCP Interleaved만 지원
    co_await send_response(461, "Unsupported Transport", req.cseq);
    co_return;
  }

  // 인터리브 채널 할당
  uint8_t rtp_channel = m_next_interleaved_channel;
  uint8_t rtcp_channel = m_next_interleaved_channel + 1;
  m_next_interleaved_channel += 2;

  // TCP Interleaved Transport 생성 (직렬화기 + 소켓 공유)
  auto transport = std::make_shared<TcpInterleavedTransport>(
    m_write_serializer,
    m_socket,
    rtp_channel,
    rtcp_channel);

  m_track_setups[track_index].rtp_transport = transport;
  m_track_setups[track_index].is_setup = true;
  m_track_setups[track_index].transport_info.transport = RtspTransport::kRtpTcp;
  m_track_setups[track_index].transport_info.interleaved_rtp = rtp_channel;
  m_track_setups[track_index].transport_info.interleaved_rtcp = rtcp_channel;

  auto response_transport
    = fmt::format("RTP/AVP/TCP;unicast;interleaved={}-{}", rtp_channel, rtcp_channel);

  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Transport", response_transport},
      {  "Session",       m_session_id}
  });

  m_state = RtspSessionState::kReady;
}

// ============================================================================
// PLAY
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_play(const ParsedRequest& req)
{
  if (!m_media_session || m_state < RtspSessionState::kReady) {
    co_await send_response(455, "Method Not Valid in This State", req.cseq);
    co_return;
  }

  // 모든 SETUP된 트랙의 transport를 미디어 세션에 전달
  for (auto& setup : m_track_setups) {
    if (setup.is_setup && setup.rtp_transport) {
      m_media_session->on_play(setup.track_index, setup.rtp_transport, m_session_id);
    }
  }

  m_state = RtspSessionState::kPlaying;

  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Session", m_session_id},
      {  "Range", "npt=0.000-"}
  });

  spdlog::info(
    "[RtspServerSession] PLAY: client={} tracks={}",
    client_address(),
    m_track_setups.size());
}

// ============================================================================
// PAUSE
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_pause(const ParsedRequest& req)
{
  if (m_state != RtspSessionState::kPlaying) {
    co_await send_response(455, "Method Not Valid in This State", req.cseq);
    co_return;
  }

  // 미디어 세션에서 transport 제거 (일시 정지)
  for (auto& setup : m_track_setups) {
    if (setup.rtp_transport) {
      m_media_session->on_teardown(
        setup.track_index,
        setup.rtp_transport,
        m_session_id);
    }
  }

  m_state = RtspSessionState::kReady;

  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Session", m_session_id}
  });
}

// ============================================================================
// TEARDOWN
// ============================================================================

nx::awaitable<void>
RtspServerSession::handle_teardown(const ParsedRequest& req)
{
  // 미디어 세션에서 모든 transport 제거
  if (m_media_session) {
    for (auto& setup : m_track_setups) {
      if (setup.rtp_transport) {
        m_media_session->on_teardown(
          setup.track_index,
          setup.rtp_transport,
          m_session_id);
        setup.rtp_transport.reset();
      }
    }
  }

  m_state = RtspSessionState::kDisconnected;

  co_await send_response(
    200,
    "OK",
    req.cseq,
    {
      {"Session", m_session_id}
  });

  spdlog::info("[RtspServerSession] TEARDOWN: client={}", client_address());

  // 연결 종료
  close();
}

// ============================================================================
// RTSP 응답 전송
// ============================================================================

nx::awaitable<void>
RtspServerSession::send_response(
  uint16_t status_code,
  const std::string& reason,
  uint32_t cseq,
  const std::map<std::string, std::string>& headers,
  const std::string& body)
{
  std::string response = fmt::format("RTSP/1.0 {} {}\r\n", status_code, reason);
  response += fmt::format("CSeq: {}\r\n", cseq);

  for (const auto& [key, value] : headers) {
    response += fmt::format("{}: {}\r\n", key, value);
  }

  if (!body.empty()) {
    response += fmt::format("Content-Length: {}\r\n", body.size());
  }

  response += "\r\n";
  response += body;

  // 직렬화기를 통해 전송 (비동기, 즉시 리턴)
  m_write_serializer->submit(std::move(response));
  co_return;
}

// ============================================================================
// 유틸리티
// ============================================================================

std::string
RtspServerSession::extract_path(const std::string& uri)
{
  // "rtsp://host:port/path" → "/path"
  auto scheme_end = uri.find("://");
  if (scheme_end == std::string::npos) {
    return uri; // 이미 경로
  }

  auto path_start = uri.find('/', scheme_end + 3);
  if (path_start == std::string::npos) {
    return "/";
  }

  return uri.substr(path_start);
}

std::string
RtspServerSession::generate_session_id()
{
  static std::mt19937_64 rng(std::random_device{}());
  auto val = rng();
  return fmt::format("{:016X}", val);
}

} // namespace nx::net
