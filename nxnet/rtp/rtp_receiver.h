// 파일: rtp_receiver.h
// 생성일: 2026-02-23
// 설명: RTP 수신기 (UDP/TCP Interleaved)

#pragma once

#include "nxnet/rtp/rtp_types.h"
#include "nxnet/rtp/rtp_depacketizer.h"
#include "nxnet/rtp/rtp_error.h"
#include "nxcore/util/type_util.h"
#include "nxcore/media/media_frame_buffer.h"

#include <nxcore/util/asio_type.h>
#include <functional>
#include <memory>
#include <span>
#include <atomic>

namespace nx::rtp {

class RtpReceiver
{
public:
  enum class TransportMode
  {
    kUdp,
    kTcpInterleaved,
    kUdpMulticast
  };

  explicit RtpReceiver(AsioContext& ioc, TransportMode mode);

  ~RtpReceiver();

  NX_NON_COPYABLE_AND_MOVABLE(RtpReceiver);

  // ========================================================================
  // 초기화
  // ========================================================================

  // UDP 모드 초기화
  nx::awaitable<std::error_code>
  bind_udp(uint16_t local_rtp_port, uint16_t local_rtcp_port);

  // TCP 인터리브드 모드: 외부에서 데이터 주입
  void feed_data(uint8_t channel, std::span<const uint8_t> data);

  // ========================================================================
  // 디패킷타이저 및 콜백
  // ========================================================================

  void set_depacketizer(std::unique_ptr<RtpDepacketizer> depacketizer);

  // 디패킷타이저 접근 (콜백 설정용)
  RtpDepacketizer* get_depacketizer() const noexcept { return m_depacketizer.get(); }

  using FrameCallback = std::function<void(
    nx::media::SharedFrameData frame, uint64_t timestamp, bool keyframe)>;

  void set_frame_callback(FrameCallback callback);

  // ========================================================================
  // Payload Type 불일치 감지
  // ========================================================================

  // SETUP 시 협상된 예상 RTP Payload Type 설정
  void set_expected_payload_type(uint8_t pt);

  // PT 불일치 콜백 (expected_pt, actual_pt)
  using PayloadTypeMismatchCallback
    = std::function<void(uint8_t expected_pt, uint8_t actual_pt)>;
  void set_payload_type_mismatch_callback(PayloadTypeMismatchCallback callback);

  // ========================================================================
  // 수신 제어
  // ========================================================================

  // UDP 수신 루프 시작
  nx::awaitable<void> start_receiving();

  // 수신 중지
  void stop();

  // ========================================================================
  // 통계
  // ========================================================================

  RtpStatistics get_statistics() const noexcept;

private:
  void handle_rtp_packet(std::span<const uint8_t> data);

  AsioContext& m_ioc;
  TransportMode m_mode;

  // UDP 소켓
  std::unique_ptr<boost::asio::ip::udp::socket> m_rtp_socket;
  std::unique_ptr<boost::asio::ip::udp::socket> m_rtcp_socket;

  // 디패킷타이저
  std::unique_ptr<RtpDepacketizer> m_depacketizer;
  FrameCallback m_frame_callback;

  // 프레임 버퍼 풀 (수신 경로 힙 할당 제거)
  std::shared_ptr<nx::media::MediaFrameBufferPool> m_frame_pool;
  // 디패킷타이저 없이 패스스루 시 사용하는 임시 버퍼
  nx::media::SharedFrameData m_passthrough_buffer;

  // 통계
  RtpStatistics m_statistics;
  uint16_t m_expected_seq = 0;
  bool m_seq_initialized = false;

  // 제어
  std::atomic<bool> m_running{false};

  // PT 불일치 감지
  uint8_t m_expected_pt = 0;
  bool m_pt_initialized = false;
  uint32_t m_pt_mismatch_frames = 0;
  uint32_t m_last_mismatch_timestamp = 0;
  bool m_pt_mismatch_reported = false;
  PayloadTypeMismatchCallback m_pt_mismatch_callback;
  static constexpr uint32_t kPtMismatchThreshold = 3;
};

} // namespace nx::rtp
