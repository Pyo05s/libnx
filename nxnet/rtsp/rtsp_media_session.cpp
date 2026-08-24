// 파일: rtsp_media_session.cpp
// 생성일: 2026-04-24
// 설명: RTSP 미디어 세션 인터페이스 기본 구현

#include "rtsp_media_session.h"

namespace nx::net {

void
IRtpTransport::send_rtp_batch(std::span<const std::vector<uint8_t>> packets)
{
  // 기본 구현: 개별 send_rtp 순차 호출 (UDP 등 배치 최적화 불필요한 transport용)
  for (const auto& pkt : packets) {
    send_rtp(std::span<const uint8_t>(pkt));
  }
}

void
IRtpTransport::send_rtp_frame(const nx::rtp::SharedRtpFrame& frame_buffer)
{
  // 기본 구현: 개별 send_rtp 순차 호출 (UDP 등 scatter-gather 미지원 transport용)
  if (!frame_buffer) {
    return;
  }
  for (size_t i = 0; i < frame_buffer->packet_count(); ++i) {
    send_rtp(frame_buffer->packet(i));
  }
}

} // namespace nx::net
