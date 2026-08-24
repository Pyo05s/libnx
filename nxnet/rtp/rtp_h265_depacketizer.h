// 파일: rtp_h265_depacketizer.h
// 생성일: 2026-04-02
// 설명: H.265 RTP 디패킷타이저 (RFC 7798)

#pragma once

#include "nxnet/rtp/rtp_depacketizer.h"
#include <array>
#include <vector>
#include <cstdint>

namespace nx::rtp {

class RtpH265Depacketizer : public RtpDepacketizer
{
public:
  RtpH265Depacketizer();

  bool process_packet(
    const RtpHeaderView& header,
    std::span<const uint8_t> payload,
    std::vector<uint8_t>& out_frame,
    bool& out_keyframe) override;

  void reset() override;

private:
  // H.265 NAL unit type (6비트, NAL 헤더 1번째 바이트의 bit[6:1])
  // RFC 7798 Section 1.1.4
  enum class NalUnitType : uint8_t
  {
    kTrailN = 0,
    kTrailR = 1,
    kBlaWLp = 16,
    kBlaWRadl = 17,
    kBlaNLp = 18,
    kIdrWRadl = 19,
    kIdrNLp = 20,
    kCraNut = 21,
    kVps = 32,
    kSps = 33,
    kPps = 34,
    kAud = 35,
    kPrefixSei = 39,
    kSuffixSei = 40,
    kAp = 48, // Aggregation Packet
    kFu = 49  // Fragmentation Unit
  };

  // Single NAL Unit 패킷 처리
  void handle_single_nal(std::span<const uint8_t> payload);

  // FU (Fragmentation Unit) 처리
  void handle_fu(std::span<const uint8_t> payload);

  // AP (Aggregation Packet) 처리
  void handle_ap(std::span<const uint8_t> payload);

  // NAL 유닛 타입별 분배 (process_packet 내부 헬퍼)
  // rtp_timestamp: 코덱 불일치 감지 시 프레임 단위 구분에 사용
  void dispatch_nal(std::span<const uint8_t> payload, uint32_t rtp_timestamp);

  // NAL 타입이 키프레임 (IRAP) 인지 확인
  static bool is_keyframe_nal(uint8_t nal_type);

  // NAL 헤더에서 타입 추출 (상위 바이트의 bit[6:1])
  static uint8_t extract_nal_type(uint8_t first_byte);

  // Annex-B 스타일 4-byte start code 추가
  static void append_start_code(std::vector<uint8_t>& buffer);

  std::vector<uint8_t> m_accumulator;   // FU 조립 버퍼
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
