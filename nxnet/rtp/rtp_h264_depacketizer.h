// 파일: rtp_h264_depacketizer.h
// 생성일: 2026-02-23
// 설명: H.264 RTP 디패킷타이저 (RFC 6184)

#pragma once

#include "nxnet/rtp/rtp_depacketizer.h"
#include <array>
#include <vector>
#include <cstdint>

namespace nx::rtp {

class RtpH264Depacketizer : public RtpDepacketizer
{
public:
  RtpH264Depacketizer();

  bool process_packet(
    const RtpHeaderView& header,
    std::span<const uint8_t> payload,
    std::vector<uint8_t>& out_frame,
    bool& out_keyframe) override;

  void reset() override;

private:
  // H.264 NAL 유닛 타입
  enum class NalUnitType : uint8_t
  {
    kSliceNonIdr = 1,
    kSliceA = 2,
    kSliceB = 3,
    kSliceC = 4,
    kSliceIdr = 5,
    kSei = 6,
    kSps = 7,
    kPps = 8,
    kAud = 9,
    kStapA = 24,
    kStapB = 25,
    kMtap16 = 26,
    kMtap24 = 27,
    kFuA = 28,
    kFuB = 29
  };

  // Single NAL Unit 패킷 처리
  void handle_single_nal(std::span<const uint8_t> payload);

  // FU-A (Fragmentation Unit) 처리
  void handle_fu_a(std::span<const uint8_t> payload);

  // STAP-A (Single-Time Aggregation Packet) 처리
  void handle_stap_a(std::span<const uint8_t> payload);

  // NAL 타입이 키프레임인지 확인 (IDR 슬라이스만 해당; SPS/PPS는 제외)
  static bool is_keyframe_nal(uint8_t nal_type);

  // Annex-B 4-byte start code 추가 (H.265 디패킷타이저와 동일 포맷)
  static void append_start_code(std::vector<uint8_t>& buffer);

  // NAL 유닛 타입별 분배 (process_packet 내부 헬퍼)
  // rtp_timestamp: 코덱 불일치 감지 시 프레임 단위 구분에 사용
  void dispatch_nal(std::span<const uint8_t> payload, uint32_t rtp_timestamp);

  std::vector<uint8_t> m_accumulator;   // FU-A 조립 버퍼
  std::vector<uint8_t> m_frame_buffer;  // 프레임 조립 버퍼
  std::vector<uint8_t> m_pending_frame; // 타임스탬프 변경 시 대기 프레임
  uint16_t m_last_seq = 0;
  uint32_t m_last_timestamp = 0;
  bool m_in_fragmentation = false;
  bool m_frame_has_keyframe = false;
  bool m_has_pending = false;
  bool m_pending_keyframe = false;
};

} // namespace nx::rtp
