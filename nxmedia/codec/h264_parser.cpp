// 파일: h264_parser.cpp
// 생성일: 2026-04-01
// 설명: H.264 SPS 파싱 구현 - Exp-Golomb 디코딩 기반

#include "nxmedia/codec/h264_parser.h"
#include "nxmedia/codec/common.h"
#include "nxmedia/codec/nal_unit_parser.h"
#include "nxcore/util/bit_util.h"

#include <algorithm>
#include <cstring>
#include <format>
#include <vector>

namespace nx::media::codec {

namespace {

/// scaling_list 건너뛰기 (SPS High profile 확장)
void
skip_scaling_list(nx::BitReader& reader, int size)
{
  int last_scale = 8;
  int next_scale = 8;

  for (int i = 0; i < size; ++i) {
    if (next_scale != 0) {
      auto delta = reader.read_se();
      next_scale = (last_scale + delta + 256) % 256;
    }
    last_scale = (next_scale == 0) ? last_scale : next_scale;
  }
}

} // anonymous namespace

std::optional<H264SpsInfo>
parse_h264_sps(std::span<const uint8_t> sps_data)
{
  if (sps_data.size() < 4) {
    return std::nullopt;
  }

  // NAL header 검증
  auto nal_type = h264_nal::type(sps_data[0]);
  if (nal_type != h264_nal::kSps) {
    return std::nullopt;
  }

  // 에뮬레이션 방지 바이트 제거
  auto rbsp = remove_emulation_prevention(sps_data.subspan(1));
  if (rbsp.size() < 3) {
    return std::nullopt;
  }

  H264SpsInfo info;

  // profile_idc(8), constraint_set_flags(8), level_idc(8)
  info.profile_idc = rbsp[0];
  info.constraint_set_flags = rbsp[1];
  info.level_idc = rbsp[2];

  nx::BitReader reader(std::span<const uint8_t>(rbsp).subspan(3));

  // seq_parameter_set_id: ue(v)
  info.sps_id = static_cast<uint8_t>(reader.read_ue());

  // High profile 계열 확장 파싱
  if (
    info.profile_idc == 100 || info.profile_idc == 110 || info.profile_idc == 122
    || info.profile_idc == 244 || info.profile_idc == 44 || info.profile_idc == 83
    || info.profile_idc == 86 || info.profile_idc == 118 || info.profile_idc == 128
    || info.profile_idc == 138 || info.profile_idc == 139 || info.profile_idc == 134
    || info.profile_idc == 135) {
    // chroma_format_idc: ue(v)
    info.chroma_format_idc = static_cast<uint8_t>(reader.read_ue());
    if (info.chroma_format_idc == 3) {
      reader.skip_bits(1); // separate_colour_plane_flag
    }

    // bit_depth_luma_minus8: ue(v)
    info.bit_depth_luma = static_cast<uint8_t>(reader.read_ue() + 8);
    // bit_depth_chroma_minus8: ue(v)
    info.bit_depth_chroma = static_cast<uint8_t>(reader.read_ue() + 8);

    // qpprime_y_zero_transform_bypass_flag: u(1)
    reader.skip_bits(1);

    // seq_scaling_matrix_present_flag: u(1)
    auto scaling_matrix_present = reader.read_bit();
    if (scaling_matrix_present) {
      int count = (info.chroma_format_idc != 3) ? 8 : 12;
      for (int i = 0; i < count; ++i) {
        auto list_present = reader.read_bit();
        if (list_present) {
          skip_scaling_list(reader, (i < 6) ? 16 : 64);
        }
      }
    }
  }

  // log2_max_frame_num_minus4: ue(v)
  info.log2_max_frame_num = static_cast<uint8_t>(reader.read_ue() + 4);

  // pic_order_cnt_type: ue(v)
  info.pic_order_cnt_type = static_cast<uint8_t>(reader.read_ue());

  if (info.pic_order_cnt_type == 0) {
    // log2_max_pic_order_cnt_lsb_minus4: ue(v)
    reader.read_ue();
  }
  else if (info.pic_order_cnt_type == 1) {
    // delta_pic_order_always_zero_flag: u(1)
    reader.skip_bits(1);
    // offset_for_non_ref_pic: se(v)
    reader.read_se();
    // offset_for_top_to_bottom_field: se(v)
    reader.read_se();
    // num_ref_frames_in_pic_order_cnt_cycle: ue(v)
    auto num_ref = reader.read_ue();
    for (uint32_t i = 0; i < num_ref; ++i) {
      reader.read_se(); // offset_for_ref_frame
    }
  }

  // max_num_ref_frames: ue(v) — 건너뛰기
  reader.read_ue();
  // gaps_in_frame_num_value_allowed_flag: u(1)
  reader.skip_bits(1);

  // pic_width_in_mbs_minus1: ue(v)
  auto pic_width_in_mbs = reader.read_ue() + 1;
  // pic_height_in_map_units_minus1: ue(v)
  auto pic_height_in_map_units = reader.read_ue() + 1;

  // frame_mbs_only_flag: u(1)
  auto frame_mbs_only = reader.read_bit();
  if (!frame_mbs_only) {
    reader.skip_bits(1); // mb_adaptive_frame_field_flag
  }

  // direct_8x8_inference_flag: u(1)
  reader.skip_bits(1);

  // frame_cropping_flag: u(1)
  uint32_t crop_left = 0, crop_right = 0, crop_top = 0, crop_bottom = 0;
  auto frame_cropping = reader.read_bit();
  if (frame_cropping) {
    crop_left = reader.read_ue();
    crop_right = reader.read_ue();
    crop_top = reader.read_ue();
    crop_bottom = reader.read_ue();
  }

  // 해상도 계산
  // chroma_format_idc에 따른 crop 단위
  uint32_t crop_unit_x = 1;
  uint32_t crop_unit_y = 2 - frame_mbs_only;

  if (info.chroma_format_idc == 1) {
    crop_unit_x = 2;
    crop_unit_y *= 2;
  }
  else if (info.chroma_format_idc == 2) {
    crop_unit_x = 2;
    crop_unit_y *= 1;
  }

  info.width = pic_width_in_mbs * 16 - crop_unit_x * (crop_left + crop_right);
  info.height = pic_height_in_map_units * 16 * (2 - frame_mbs_only)
                - crop_unit_y * (crop_top + crop_bottom);

  return info;
}

std::string
to_avc_codec_string(const H264SpsInfo& sps)
{
  // "avc1.XXYYZZ" — XX=profile_idc, YY=constraint_set_flags, ZZ=level_idc
  return std::format(
    "avc1.{:02X}{:02X}{:02X}",
    sps.profile_idc,
    sps.constraint_set_flags,
    sps.level_idc);
}

std::optional<H264SpsInfo>
find_and_parse_h264_sps(std::span<const uint8_t> data)
{
  auto nal_units = parse_nal_units(data);

  for (const auto& nal : nal_units) {
    if (h264_nal::type(nal.type) == h264_nal::kSps) {
      return parse_h264_sps(nal.data);
    }
  }

  return std::nullopt;
}

} // namespace nx::media::codec
