// 파일: h264_parser.h
// 생성일: 2026-04-01
// 설명: H.264 SPS/PPS 파싱 - profile/level/해상도 추출

#pragma once

#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace nx::media::codec {

/// H.264 NAL unit type 상수
namespace h264_nal {

constexpr uint8_t kSliceNonIdr = 1;
constexpr uint8_t kSliceIdr = 5;
constexpr uint8_t kSei = 6;
constexpr uint8_t kSps = 7;
constexpr uint8_t kPps = 8;

/// NAL unit type 추출 (하위 5비트)
constexpr uint8_t
type(uint8_t nal_header)
{
  return nal_header & 0x1F;
}

} // namespace h264_nal

/// H.264 SPS에서 추출한 코덱 파라미터
struct H264SpsInfo
{
  uint8_t profile_idc = 0;          // Baseline=66, Main=77, High=100
  uint8_t constraint_set_flags = 0; // constraint_set0~5_flag
  uint8_t level_idc = 0;            // 30=3.0, 31=3.1, 40=4.0, 51=5.1
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t sps_id = 0;
  uint8_t chroma_format_idc = 1; // 0=모노, 1=4:2:0, 2=4:2:2, 3=4:4:4
  uint8_t bit_depth_luma = 8;
  uint8_t bit_depth_chroma = 8;
  uint8_t log2_max_frame_num = 0;
  uint8_t pic_order_cnt_type = 0;
};

/// SPS NAL unit 바이트에서 파라미터 추출
/// @param sps_data SPS NAL unit (start code 제외, nal_unit_type 바이트 포함)
/// @return 파싱 성공 시 SPS 정보, 실패 시 nullopt
std::optional<H264SpsInfo> parse_h264_sps(std::span<const uint8_t> sps_data);

/// SPS 정보에서 WebCodecs 코덱 문자열 생성
/// @return "avc1.XXYYZZ" 형식 (예: "avc1.64001F")
std::string to_avc_codec_string(const H264SpsInfo& sps);

/// Annex B 바이트스트림에서 SPS NAL unit을 찾아 파싱
/// @param data Annex B 형식 (keyframe 데이터 등)
/// @return SPS 정보, 없으면 nullopt
std::optional<H264SpsInfo> find_and_parse_h264_sps(std::span<const uint8_t> data);

} // namespace nx::media::codec
