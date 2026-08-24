// 파일: rtp_packetizer.cpp
// 생성일: 2026-03-30
// 설명: RTP 패킷타이저 인터페이스 기본 구현

#include "rtp_packetizer.h"

namespace nx::rtp {

void
RtpPacketizer::packetize(
  std::span<const uint8_t> frame_data,
  uint32_t timestamp,
  bool is_keyframe,
  const RtpHeaderExtensionBuilder& /*ext*/,
  RtpFrameBuffer& output)
{
  // 기본 구현: 확장 헤더 무시, 기존 packetize 호출
  packetize(frame_data, timestamp, is_keyframe, output);
}

} // namespace nx::rtp
