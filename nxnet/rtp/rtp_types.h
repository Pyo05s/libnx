// 파일: rtp_types.h
// 생성일: 2026-02-23
// 설명: RTP 공통 타입 정의

#pragma once

#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace nx::rtp {

// RTP 페이로드 타입 (동적 타입은 96 이상)
enum class RtpPayloadType : uint8_t
{
  kPcmu = 0,
  kGsm = 3,
  kG723 = 4,
  kPcma = 8,
  kL16Stereo = 10,
  kL16Mono = 11,
  kJpeg = 26,
  kDynamic = 96 // 동적 타입 시작
};

// ============================================================================
// RFC 5285 RTP One-Byte Header Extension
// ============================================================================

// One-Byte Header Extension 프로필 식별자 (RFC 5285)
inline constexpr uint16_t kOneByteExtensionProfile = 0xBEDE;

// RFC 5285 One-Byte Header Extension 요소
struct RtpHeaderExtElement
{
  uint8_t id = 0;            // 확장 ID (1~14)
  std::vector<uint8_t> data; // 확장 데이터 (1~16 bytes)
};

// RFC 6051: NTP-64 확장 URI
inline constexpr std::string_view kExtmapUriNtp64
  = "urn:ietf:params:rtp-hdrext:ntp-64";

// RTP 확장 헤더 (파싱된 원시 블록)
struct RtpExtension
{
  uint16_t profile = 0;
  std::vector<uint8_t> data;
};

// RTP 헤더 구조
struct RtpHeader
{
  uint8_t version = 2;
  bool padding = false;
  bool extension = false;
  uint8_t csrc_count = 0;
  bool marker = false;
  uint8_t payload_type = 0;
  uint16_t sequence_number = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;
  std::vector<uint32_t> csrc_list;
  std::optional<RtpExtension> ext_header;
};

// RTP 헤더 경량 뷰 (핫패스용 — 힙 할당 없음)
// CSRC/확장 헤더는 offset 스킵만 수행하며 파싱하지 않음
struct RtpHeaderView
{
  uint8_t version = 2;
  bool padding = false;
  bool extension = false;
  bool marker = false;
  uint8_t payload_type = 0;
  uint16_t sequence_number = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;
};

// RTP 수신 통계
struct RtpStatistics
{
  uint64_t packets_received = 0;
  uint64_t packets_lost = 0;
  uint64_t packets_out_of_order = 0;
  double jitter_ms = 0.0;
  uint64_t bytes_received = 0;
};

} // namespace nx::rtp
