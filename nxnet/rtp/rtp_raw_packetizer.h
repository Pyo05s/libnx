// 파일: rtp_raw_packetizer.h
// 생성일: 2026-03-18
// 설명: Raw RTP 패킷타이저 — 코덱 비의존 패스스루 패킷타이저

#pragma once

#include "nxnet/rtp/rtp_packetizer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nx::rtp {

/// Raw RTP 패킷타이저
/// - 코덱에 독립적인 범용 패킷타이저
/// - 원시 프레임 데이터를 MTU 단위로 분할하여 RTP 패킷으로 전송
/// - SDP application/metadata 등 비표준 미디어 트랙 중계에 사용
/// - 코덱 변환 없이 원본 데이터를 그대로 RTP 페이로드에 삽입
class RtpRawPacketizer : public RtpPacketizer
{
public:
  /// @param max_packet_size 최대 RTP 패킷 크기 (기본 1400 bytes)
  explicit RtpRawPacketizer(uint16_t max_packet_size = 1400);

  void packetize(
    std::span<const uint8_t> frame_data,
    uint32_t timestamp,
    bool is_keyframe,
    RtpFrameBuffer& output) override;

  void packetize(
    std::span<const uint8_t> frame_data,
    uint32_t timestamp,
    bool is_keyframe,
    const RtpHeaderExtensionBuilder& ext,
    RtpFrameBuffer& output) override;

  void reset() override;
  void set_ssrc(uint32_t ssrc) override;
  void set_payload_type(uint8_t pt) override;
  void set_max_packet_size(uint16_t size) override;

private:
  /// RTP 헤더를 RtpFrameBuffer에 직접 기록
  void write_rtp_header(RtpFrameBuffer& output, bool marker, uint32_t timestamp);

  uint32_t m_ssrc = 0;
  uint8_t m_payload_type = 96; // 동적 페이로드 기본값
  uint16_t m_sequence_number = 0;
  uint16_t m_max_payload_size; // RTP 헤더 제외 페이로드 최대 크기

  // 현재 패킷화 중인 확장 헤더 (packetize 호출 동안만 유효)
  const RtpHeaderExtensionBuilder* m_current_ext = nullptr;

  static constexpr uint16_t kRtpHeaderSize = 12;
};

} // namespace nx::rtp
