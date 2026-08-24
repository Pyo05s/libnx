// 파일: rtp_h264_packetizer.h
// 생성일: 2026-02-26
// 설명: H.264 RTP 패킷타이저 (RFC 6184)

#pragma once

#include "nxnet/rtp/rtp_packetizer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nx::rtp {

/// H.264 RTP 패킷타이저
/// - Single NAL Unit: NAL 크기가 MTU 이내일 때 그대로 전송
/// - FU-A (Fragmentation Unit): 큰 NAL을 MTU 단위로 분할
/// - STAP-A (Aggregation): 작은 NAL 여러 개를 하나의 패킷으로 집합
/// - Annex B Start Code (0x00000001 / 0x000001) 자동 제거
class RtpH264Packetizer : public RtpPacketizer
{
public:
  /// @param max_packet_size 최대 RTP 페이로드 크기 (기본 1400 bytes)
  explicit RtpH264Packetizer(uint16_t max_packet_size = 1400);

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

  /// 프레임 데이터에서 NAL 유닛 목록 추출 (내부 유통 포맷: Annex B)
  static std::vector<std::span<const uint8_t>>
  extract_nal_units(std::span<const uint8_t> frame_data);

private:
  /// Annex B 스타일 (Start Code 기반) NAL 추출
  static std::vector<std::span<const uint8_t>>
  extract_nal_units_annex_b(std::span<const uint8_t> frame_data);

  /// Single NAL Unit 패킷을 RtpFrameBuffer에 직접 기록
  void send_single_nal(
    std::span<const uint8_t> nal,
    uint32_t timestamp,
    bool marker,
    RtpFrameBuffer& output);

  /// FU-A 분할 패킷을 RtpFrameBuffer에 직접 기록
  void send_fu_a(
    std::span<const uint8_t> nal,
    uint32_t timestamp,
    bool marker,
    RtpFrameBuffer& output);

  /// STAP-A 집합 패킷을 RtpFrameBuffer에 직접 기록
  void send_stap_a(
    const std::vector<std::span<const uint8_t>>& nals,
    uint32_t timestamp,
    bool marker,
    RtpFrameBuffer& output);

  /// RTP 헤더를 RtpFrameBuffer에 직접 기록 (12 bytes 고정, CSRC/확장 미사용)
  void write_rtp_header(RtpFrameBuffer& output, bool marker, uint32_t timestamp);

  // NAL 타입 상수
  static constexpr uint8_t kNalStapA = 24;
  static constexpr uint8_t kNalFuA = 28;

  uint32_t m_ssrc = 0;
  uint8_t m_payload_type = 96; // 동적 페이로드 타입
  uint16_t m_sequence_number = 0;
  uint16_t m_max_payload_size; // RTP 헤더 제외 페이로드 최대 크기

  // 현재 패킷화 중인 확장 헤더 (packetize 호출 동안만 유효)
  const RtpHeaderExtensionBuilder* m_current_ext = nullptr;

  // RTP 헤더 크기 (12 bytes 고정)
  static constexpr uint16_t kRtpHeaderSize = 12;
};

} // namespace nx::rtp
