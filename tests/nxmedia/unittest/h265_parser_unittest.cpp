// 파일: h265_parser_unittest.cpp
// 생성일: 2026-04-01
// 설명: H.265 SPS 파서 단위 테스트

#include <gtest/gtest.h>

#include "nxmedia/codec/h265_parser.h"
#include "nxmedia/codec/nal_unit_parser.h"

#include <vector>

namespace codec = nx::media::codec;

class H265ParserTest : public ::testing::Test
{};

// === H.265 NAL type 유틸리티 ===
TEST_F(H265ParserTest, NalType_Extraction)
{
  // H.265 NAL 헤더 첫 바이트: forbidden(1) + type(6) + layer_id_high(1)
  // SPS: type=33 → (33 << 1) = 0x42
  EXPECT_EQ(codec::h265_nal::type(0x42), codec::h265_nal::kSps);

  // VPS: type=32 → (32 << 1) = 0x40
  EXPECT_EQ(codec::h265_nal::type(0x40), codec::h265_nal::kVps);

  // PPS: type=34 → (34 << 1) = 0x44
  EXPECT_EQ(codec::h265_nal::type(0x44), codec::h265_nal::kPps);

  // IDR_W_RADL: type=19 → (19 << 1) = 0x26
  EXPECT_EQ(codec::h265_nal::type(0x26), codec::h265_nal::kIdrWRadl);

  // IDR_N_LP: type=20 → (20 << 1) = 0x28
  EXPECT_EQ(codec::h265_nal::type(0x28), codec::h265_nal::kIdrNLp);
}

TEST_F(H265ParserTest, IsIdr)
{
  EXPECT_TRUE(codec::h265_nal::is_idr(codec::h265_nal::kIdrWRadl));
  EXPECT_TRUE(codec::h265_nal::is_idr(codec::h265_nal::kIdrNLp));
  EXPECT_FALSE(codec::h265_nal::is_idr(codec::h265_nal::kTrailR));
  EXPECT_FALSE(codec::h265_nal::is_idr(codec::h265_nal::kSps));
}

// === 코덱 문자열 생성 ===
TEST_F(H265ParserTest, ToHevcCodecString_Main_Level31)
{
  codec::H265SpsInfo sps;
  sps.general_profile_idc = 1;                             // Main
  sps.general_tier_flag = 0;                               // Main tier
  sps.general_level_idc = 93;                              // Level 3.1
  sps.general_profile_compatibility_flags = 0x60000000;    // bit 5,6
  sps.general_constraint_indicator_flags = 0x900000000000; // constraint byte

  auto codec_string = codec::to_hevc_codec_string(sps);
  // hev1.1.{compat}.L93.{constraint} 형식 검증
  EXPECT_TRUE(codec_string.starts_with("hev1.1."));
  EXPECT_TRUE(codec_string.find("L93") != std::string::npos);
}

TEST_F(H265ParserTest, ToHevcCodecString_Main10_HighTier)
{
  codec::H265SpsInfo sps;
  sps.general_profile_idc = 2;                          // Main 10
  sps.general_tier_flag = 1;                            // High tier
  sps.general_level_idc = 153;                          // Level 5.1
  sps.general_profile_compatibility_flags = 0x20000000; // bit 5
  sps.general_constraint_indicator_flags = 0;

  auto codec_string = codec::to_hevc_codec_string(sps);
  // High tier → 'H'
  EXPECT_TRUE(codec_string.find("H153") != std::string::npos);
  EXPECT_TRUE(codec_string.starts_with("hev1.2."));
}

// === H.265 SPS 파싱 테스트 ===
TEST_F(H265ParserTest, ParseSps_Main_1920x1080)
{
  // H.265 SPS NAL: type=33, Main profile, Level 4.0, 1920x1080
  // NAL 헤더: 0x42 0x01 (SPS, nuh_temporal_id_plus1=1)
  std::vector<uint8_t> sps_nal = {
    0x42,
    0x01, // NAL header (type=33=SPS)
    0x01, // sps_vps_id=0, max_sub_layers_minus1=0, temporal_id_nesting=1
    // profile_tier_level:
    0x01, // profile_space=0, tier=0, profile_idc=1 (Main)
    0x60,
    0x00,
    0x00,
    0x00, // profile_compatibility_flags
    0x90,
    0x00,
    0x00,
    0x00,
    0x00,
    0x00, // constraint (48bits)
    0x78, // general_level_idc = 120 (Level 4.0)
    // SPS body (Exp-Golomb encoded)
    0x00, // sps_seq_parameter_set_id=0
    0x00, // chroma_format_idc=1 (as ue)
    // pic_width: 1920 (ue), pic_height: 1080 (ue)
    0x02,
    0x1C,
    0x4D,
    0x94,
    0x62,
    0x08};

  auto result = codec::parse_h265_sps(sps_nal);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->general_profile_idc, 1); // Main
  EXPECT_EQ(result->general_tier_flag, 0);   // Main tier
  EXPECT_EQ(result->general_level_idc, 120); // Level 4.0
}

// === 잘못된 입력 ===
TEST_F(H265ParserTest, ParseSps_InvalidNalType)
{
  // VPS NAL type (type=32) → SPS 파싱 실패
  std::vector<uint8_t> vps_nal = {0x40, 0x01, 0x0C, 0x01, 0xFF};
  auto result = codec::parse_h265_sps(vps_nal);
  EXPECT_FALSE(result.has_value());
}

TEST_F(H265ParserTest, ParseSps_TooShort)
{
  std::vector<uint8_t> data = {0x42, 0x01};
  auto result = codec::parse_h265_sps(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(H265ParserTest, ParseSps_EmptyData)
{
  std::vector<uint8_t> data;
  auto result = codec::parse_h265_sps(data);
  EXPECT_FALSE(result.has_value());
}

// === find_and_parse_h265_sps 테스트 ===
TEST_F(H265ParserTest, FindAndParseSps_NoSps)
{
  // VPS만 존재 (type=32, header=0x40 0x01)
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x40, 0x01, 0x0C, 0x01, 0xFF};

  auto result = codec::find_and_parse_h265_sps(data);
  EXPECT_FALSE(result.has_value());
}
