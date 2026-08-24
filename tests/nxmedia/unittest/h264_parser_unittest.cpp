// 파일: h264_parser_unittest.cpp
// 생성일: 2026-04-01
// 설명: H.264 SPS 파서 단위 테스트

#include <gtest/gtest.h>

#include "nxmedia/codec/h264_parser.h"
#include "nxmedia/codec/nal_unit_parser.h"

#include <vector>

namespace codec = nx::media::codec;

class H264ParserTest : public ::testing::Test
{};

// === H.264 Baseline Profile Level 3.0 (640x480) ===
// SPS NAL unit: profile_idc=66(Baseline), constraint=0xC0, level=30
TEST_F(H264ParserTest, ParseSps_Baseline_640x480)
{
  // 실제 카메라에서 추출한 SPS 바이트: Baseline, Level 3.0, 640x480
  // nal_unit_type=0x67, profile_idc=0x42(66), constraint=0xC0, level=0x1E(30)
  // pic_width_in_mbs=40 → 640, pic_height_in_map_units=30 → 480
  std::vector<uint8_t> sps_nal = {
    0x67, // NAL header: nal_ref_idc=3, nal_unit_type=7 (SPS)
    0x42, // profile_idc = 66 (Baseline)
    0xC0, // constraint_set_flags
    0x1E, // level_idc = 30 (Level 3.0)
    0xD9, // SPS 파라미터 (Exp-Golomb 인코딩된 데이터)
    0x00,
    0xA0,
    0x47,
    0xFE,
    0x88};

  auto result = codec::parse_h264_sps(sps_nal);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->profile_idc, 66);
  EXPECT_EQ(result->constraint_set_flags, 0xC0);
  EXPECT_EQ(result->level_idc, 30);
}

// === H.264 High Profile Level 4.0 (1920x1080) ===
TEST_F(H264ParserTest, ParseSps_High_1920x1080)
{
  // High profile SPS: profile_idc=100, level=40, 1920x1080
  // nal_header=0x67, profile=0x64(100), constraint=0x00, level=0x28(40)
  // 이후 Exp-Golomb 인코딩된 파라미터
  std::vector<uint8_t> sps_nal
    = {0x67, // NAL header
       0x64, // profile_idc = 100 (High)
       0x00, // constraint_set_flags
       0x28, // level_idc = 40 (Level 4.0)
       0xAD, // sps_id=0, chroma=1, bit_depth_luma=0, bit_depth_chroma=0
       0x84, // 추가 파라미터
       0x01, 0x0C, 0x20, 0x08, 0x61, 0x00, 0x43, 0x08, 0x02, 0x18, 0x40, 0x10,
       0xC2, 0x00, 0x84, 0x2B, 0x50, 0x3C, 0x01, 0x13, 0xF2, 0xC2, 0x00};

  auto result = codec::parse_h264_sps(sps_nal);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->profile_idc, 100);
  EXPECT_EQ(result->level_idc, 40);
}

// === 코덱 문자열 생성 ===
TEST_F(H264ParserTest, ToAvcCodecString_Baseline)
{
  codec::H264SpsInfo sps;
  sps.profile_idc = 66; // Baseline
  sps.constraint_set_flags = 0xC0;
  sps.level_idc = 30; // Level 3.0

  auto codec_string = codec::to_avc_codec_string(sps);
  EXPECT_EQ(codec_string, "avc1.42C01E");
}

TEST_F(H264ParserTest, ToAvcCodecString_High)
{
  codec::H264SpsInfo sps;
  sps.profile_idc = 100; // High
  sps.constraint_set_flags = 0x00;
  sps.level_idc = 31; // Level 3.1

  auto codec_string = codec::to_avc_codec_string(sps);
  EXPECT_EQ(codec_string, "avc1.64001F");
}

TEST_F(H264ParserTest, ToAvcCodecString_Main)
{
  codec::H264SpsInfo sps;
  sps.profile_idc = 77; // Main
  sps.constraint_set_flags = 0x40;
  sps.level_idc = 40; // Level 4.0

  auto codec_string = codec::to_avc_codec_string(sps);
  EXPECT_EQ(codec_string, "avc1.4D4028");
}

TEST_F(H264ParserTest, ToAvcCodecString_HighLevel51)
{
  codec::H264SpsInfo sps;
  sps.profile_idc = 100;
  sps.constraint_set_flags = 0x00;
  sps.level_idc = 51; // Level 5.1

  auto codec_string = codec::to_avc_codec_string(sps);
  EXPECT_EQ(codec_string, "avc1.640033");
}

// === 잘못된 입력 ===
TEST_F(H264ParserTest, ParseSps_InvalidNalType)
{
  // PPS NAL type (0x68) → SPS 파싱 실패
  std::vector<uint8_t> pps_nal = {0x68, 0xCE, 0x38, 0x80};
  auto result = codec::parse_h264_sps(pps_nal);
  EXPECT_FALSE(result.has_value());
}

TEST_F(H264ParserTest, ParseSps_TooShort)
{
  std::vector<uint8_t> data = {0x67, 0x42};
  auto result = codec::parse_h264_sps(data);
  EXPECT_FALSE(result.has_value());
}

TEST_F(H264ParserTest, ParseSps_EmptyData)
{
  std::vector<uint8_t> data;
  auto result = codec::parse_h264_sps(data);
  EXPECT_FALSE(result.has_value());
}

// === find_and_parse_h264_sps 테스트 ===
TEST_F(H264ParserTest, FindAndParseSps_WithStartCodes)
{
  // Annex B: SPS + PPS
  std::vector<uint8_t> data = {
    0x00, 0x00, 0x00, 0x01, // start code
    0x67,                   // SPS NAL header
    0x42, 0xC0, 0x1E,       // profile=66, constraint=0xC0, level=30
    0xD9, 0x00, 0xA0, 0x47, 0xFE, 0x88, 0x00, 0x00, 0x00, 0x01, // start code
    0x68, 0xCE, 0x38, 0x80                                      // PPS
  };

  auto result = codec::find_and_parse_h264_sps(data);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->profile_idc, 66);
  EXPECT_EQ(result->level_idc, 30);
}

TEST_F(H264ParserTest, FindAndParseSps_NoSps)
{
  // PPS만 존재
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x38, 0x80};

  auto result = codec::find_and_parse_h264_sps(data);
  EXPECT_FALSE(result.has_value());
}
