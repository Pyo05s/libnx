// 파일: h265_parser.cpp
// 생성일: 2026-04-01
// 설명: H.265 SPS 파싱 구현 - profile_tier_level 구조 디코딩

#include "nxmedia/codec/h265_parser.h"
#include "nxmedia/codec/common.h"
#include "nxmedia/codec/nal_unit_parser.h"
#include "nxcore/util/bit_util.h"

#include <algorithm>
#include <format>
#include <vector>

namespace nx::media::codec {

namespace {

/// profile_tier_level 구조 파싱
void
parse_profile_tier_level(nx::BitReader& reader, H265SpsInfo& info, int max_sub_layers)
{
  // general_profile_space: u(2)
  reader.skip_bits(2);
  // general_tier_flag: u(1)
  info.general_tier_flag = static_cast<uint8_t>(reader.read_bit());
  // general_profile_idc: u(5)
  info.general_profile_idc = static_cast<uint8_t>(reader.read_bits(5));

  // general_profile_compatibility_flag[32]: u(32)
  info.general_profile_compatibility_flags = reader.read_bits(32);

  // general_constraint_indicator_flags: u(48) = 6바이트
  uint64_t constraint_flags = 0;
  constraint_flags = static_cast<uint64_t>(reader.read_bits(32)) << 16;
  constraint_flags |= reader.read_bits(16);
  info.general_constraint_indicator_flags = constraint_flags;

  // general_level_idc: u(8)
  info.general_level_idc = static_cast<uint8_t>(reader.read_bits(8));

  // sub_layer 정보 건너뛰기
  if (max_sub_layers > 1) {
    // sub_layer_profile_present_flag[i], sub_layer_level_present_flag[i]
    std::vector<bool> sub_profile_present(max_sub_layers - 1);
    std::vector<bool> sub_level_present(max_sub_layers - 1);

    for (int i = 0; i < max_sub_layers - 1; ++i) {
      sub_profile_present[i] = reader.read_bit() != 0;
      sub_level_present[i] = reader.read_bit() != 0;
    }

    // 패딩 (max_sub_layers < 8일 때 2*(8-max_sub_layers) 비트)
    if (max_sub_layers < 8) {
      reader.skip_bits(2 * (8 - max_sub_layers));
    }

    for (int i = 0; i < max_sub_layers - 1; ++i) {
      if (sub_profile_present[i]) {
        // sub_layer_profile_space, tier, idc, compatibility, constraint,
        // level
        reader.skip_bits(2 + 1 + 5 + 32 + 48);
      }
      if (sub_level_present[i]) {
        reader.skip_bits(8); // sub_layer_level_idc
      }
    }
  }
}

} // anonymous namespace

std::optional<H265SpsInfo>
parse_h265_sps(std::span<const uint8_t> sps_data)
{
  // H.265 NAL 헤더는 2바이트
  if (sps_data.size() < 4) {
    return std::nullopt;
  }

  // NAL type 검증
  auto nal_type = h265_nal::type(sps_data[0]);
  if (nal_type != h265_nal::kSps) {
    return std::nullopt;
  }

  // 에뮬레이션 방지 바이트 제거 (2바이트 NAL 헤더 이후부터)
  auto rbsp = remove_emulation_prevention(sps_data.subspan(2));
  if (rbsp.size() < 4) {
    return std::nullopt;
  }

  nx::BitReader reader(rbsp);
  H265SpsInfo info;

  // sps_video_parameter_set_id: u(4)
  reader.skip_bits(4);
  // sps_max_sub_layers_minus1: u(3)
  info.sps_max_sub_layers = static_cast<uint8_t>(reader.read_bits(3) + 1);
  // sps_temporal_id_nesting_flag: u(1)
  reader.skip_bits(1);

  // profile_tier_level
  parse_profile_tier_level(reader, info, info.sps_max_sub_layers);

  // sps_seq_parameter_set_id: ue(v)
  reader.read_ue();

  // chroma_format_idc: ue(v)
  info.chroma_format_idc = static_cast<uint8_t>(reader.read_ue());
  if (info.chroma_format_idc == 3) {
    reader.skip_bits(1); // separate_colour_plane_flag
  }

  // pic_width_in_luma_samples: ue(v)
  info.width = reader.read_ue();
  // pic_height_in_luma_samples: ue(v)
  info.height = reader.read_ue();

  // conformance_window_flag: u(1)
  auto conformance_window = reader.read_bit();
  if (conformance_window) {
    // crop 값으로 해상도 보정
    uint32_t crop_left = reader.read_ue();
    uint32_t crop_right = reader.read_ue();
    uint32_t crop_top = reader.read_ue();
    uint32_t crop_bottom = reader.read_ue();

    // chroma_format_idc에 따른 crop 단위
    uint32_t sub_width_c
      = (info.chroma_format_idc == 1 || info.chroma_format_idc == 2) ? 2 : 1;
    uint32_t sub_height_c = (info.chroma_format_idc == 1) ? 2 : 1;

    info.width -= (crop_left + crop_right) * sub_width_c;
    info.height -= (crop_top + crop_bottom) * sub_height_c;
  }

  // bit_depth_luma_minus8: ue(v)
  info.bit_depth_luma = static_cast<uint8_t>(reader.read_ue() + 8);
  // bit_depth_chroma_minus8: ue(v)
  info.bit_depth_chroma = static_cast<uint8_t>(reader.read_ue() + 8);

  return info;
}

std::string
to_hevc_codec_string(const H265SpsInfo& sps)
{
  // "hev1.{profile}.{compatibility}.{tier}{level}.{constraint}"

  // profile compatibility flags → 역순 비트 (RFC 6381 규격)
  uint32_t reversed = 0;
  for (int i = 0; i < 32; ++i) {
    if (sps.general_profile_compatibility_flags & (1u << i)) {
      reversed |= (1u << (31 - i));
    }
  }

  // tier: L=Main, H=High
  char tier_char = (sps.general_tier_flag == 0) ? 'L' : 'H';

  // constraint indicator flags (48비트) → 후행 0 바이트 제거
  uint8_t constraint_bytes[6];
  for (int i = 0; i < 6; ++i) {
    constraint_bytes[i] = static_cast<uint8_t>(
      (sps.general_constraint_indicator_flags >> (40 - 8 * i)) & 0xFF);
  }

  // 후행 0 바이트 수 결정 (최소 1바이트는 출력)
  int constraint_len = 6;
  while (constraint_len > 1 && constraint_bytes[constraint_len - 1] == 0) {
    --constraint_len;
  }

  std::string constraint_str;
  for (int i = 0; i < constraint_len; ++i) {
    if (i > 0) {
      constraint_str += '.';
    }
    constraint_str += std::format("{:X}", constraint_bytes[i]);
  }

  return std::format(
    "hev1.{}.{:X}.{}{}.{}",
    sps.general_profile_idc,
    reversed,
    tier_char,
    sps.general_level_idc,
    constraint_str);
}

std::optional<H265SpsInfo>
find_and_parse_h265_sps(std::span<const uint8_t> data)
{
  auto nal_units = parse_nal_units(data);

  for (const auto& nal : nal_units) {
    // H.265 NAL type은 첫 바이트의 상위 비트
    if (h265_nal::type(nal.type) == h265_nal::kSps) {
      return parse_h265_sps(nal.data);
    }
  }

  return std::nullopt;
}

} // namespace nx::media::codec
