// 파일: h265_parser.h
// 생성일: 2026-04-01
// 설명: H.265 VPS/SPS/PPS 파싱 - profile/level/tier 추출

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace nx::media::codec {

/// H.265 NAL unit type 상수
namespace h265_nal {

constexpr uint8_t kTrailN = 0;
constexpr uint8_t kTrailR = 1;
constexpr uint8_t kIdrWRadl = 19;
constexpr uint8_t kIdrNLp = 20;
constexpr uint8_t kCraNut = 21;
constexpr uint8_t kVps = 32;
constexpr uint8_t kSps = 33;
constexpr uint8_t kPps = 34;

/// NAL unit type 추출 (상위 6비트의 바이트1 >> 1)
/// H.265 NAL 헤더는 2바이트: forbidden(1) + type(6) + layer_id(6) + tid(3)
constexpr uint8_t
type(uint8_t nal_header)
{
  return (nal_header >> 1) & 0x3F;
}

/// IDR 프레임 여부
constexpr bool
is_idr(uint8_t nal_type)
{
  return nal_type == kIdrWRadl || nal_type == kIdrNLp;
}

} // namespace h265_nal

/// H.265 SPS에서 추출한 코덱 파라미터
struct H265SpsInfo
{
  uint8_t general_profile_idc = 0; // 1=Main, 2=Main10, 3=MainStillPicture
  uint8_t general_tier_flag = 0;   // 0=Main tier, 1=High tier
  uint8_t general_level_idc = 0;   // 93=3.1, 120=4.0, 153=5.1
  uint32_t general_profile_compatibility_flags = 0;
  uint64_t general_constraint_indicator_flags = 0;
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t chroma_format_idc = 1;
  uint8_t bit_depth_luma = 8;
  uint8_t bit_depth_chroma = 8;
  uint8_t sps_max_sub_layers = 1;
};

/// SPS NAL unit 바이트에서 파라미터 추출
/// @param sps_data SPS NAL unit (start code 제외, 2바이트 NAL 헤더 포함)
/// @return 파싱 성공 시 SPS 정보, 실패 시 nullopt
std::optional<H265SpsInfo> parse_h265_sps(std::span<const uint8_t> sps_data);

/// SPS 정보에서 WebCodecs 코덱 문자열 생성
/// @return "hev1.X.Y.LZZZ.BB" 형식 (예: "hev1.1.6.L93.B0")
std::string to_hevc_codec_string(const H265SpsInfo& sps);

/// Annex B 바이트스트림에서 SPS NAL unit을 찾아 파싱
/// @param data Annex B 형식 (keyframe 데이터 등)
/// @return SPS 정보, 없으면 nullopt
std::optional<H265SpsInfo> find_and_parse_h265_sps(std::span<const uint8_t> data);

} // namespace nx::media::codec
