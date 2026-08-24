// 파일: rtp_g711_packetizer.h
// 생성일: 2026-03-03
// 설명: G.711 RTP 패킷타이저 (RFC 3551) — PCMA/PCMU 지원

#pragma once

#include "nxnet/rtp/rtp_packetizer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nx::rtp {

/// G.711 RTP 패킷타이저 (PCMA / PCMU)
/// - RFC 3551 기반, 정적 페이로드 타입 (PCMU=0, PCMA=8)
/// - RTP 클럭 레이트: 8,000 Hz (1 샘플 = 1 바이트)
/// - 기본 패킷화 단위: 20ms (160 샘플 = 160 bytes)
/// - 코덱 변환 없이 원시 G.711 샘플을 그대로 RTP 페이로드에 삽입
///
/// 패킷화 전략:
/// - frame_data 크기가 max_payload_size 이하이면 단일 RTP 패킷으로 전송
/// - frame_data 크기가 max_payload_size를 초과하면 MTU 단위로 분할
/// - 마지막 패킷에 marker bit를 설정하여 프레임 경계 표시
class RtpG711Packetizer : public RtpPacketizer
{
public:
  /// @param max_packet_size 최대 RTP 패킷 크기 (기본 1400 bytes)
  explicit RtpG711Packetizer(uint16_t max_packet_size = 1400);

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
  uint8_t m_payload_type = 8; // 기본 PCMA (G.711 A-law)
  uint16_t m_sequence_number = 0;
  uint16_t m_max_payload_size; // RTP 헤더 제외 페이로드 최대 크기

  // 현재 패킷화 중인 확장 헤더 (packetize 호출 동안만 유효)
  const RtpHeaderExtensionBuilder* m_current_ext = nullptr;

  // RTP 헤더 크기 (12 bytes 고정)
  static constexpr uint16_t kRtpHeaderSize = 12;
};

} // namespace nx::rtp
