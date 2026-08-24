// 파일: nal_unit_parser_unittest.cpp
// 생성일: 2026-04-01
// 설명: NAL unit 파서 단위 테스트

#include <gtest/gtest.h>

#include "nxmedia/codec/nal_unit_parser.h"

#include <vector>

namespace codec = nx::media::codec;

class NalUnitParserTest : public ::testing::Test
{};

// === find_start_code 테스트 ===

TEST_F(NalUnitParserTest, FindStartCode_4Byte)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67};
  auto pos = codec::find_start_code(data);
  EXPECT_EQ(pos, 0u);
}

TEST_F(NalUnitParserTest, FindStartCode_3Byte)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x01, 0x67};
  auto pos = codec::find_start_code(data);
  EXPECT_EQ(pos, 0u);
}

TEST_F(NalUnitParserTest, FindStartCode_WithOffset)
{
  std::vector<uint8_t> data = {0xAA, 0xBB, 0x00, 0x00, 0x00, 0x01, 0x67};
  auto pos = codec::find_start_code(data, 1);
  EXPECT_EQ(pos, 2u);
}

TEST_F(NalUnitParserTest, FindStartCode_NotFound)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x02, 0x67};
  auto pos = codec::find_start_code(data);
  EXPECT_EQ(pos, codec::npos);
}

TEST_F(NalUnitParserTest, FindStartCode_EmptyData)
{
  std::vector<uint8_t> data;
  auto pos = codec::find_start_code(data);
  EXPECT_EQ(pos, codec::npos);
}

TEST_F(NalUnitParserTest, FindStartCode_TooShort)
{
  std::vector<uint8_t> data = {0x00, 0x01};
  auto pos = codec::find_start_code(data);
  EXPECT_EQ(pos, codec::npos);
}

TEST_F(NalUnitParserTest, FindStartCode_Multiple)
{
  // 두 개의 start code
  std::vector<uint8_t> data
    = {0x00, 0x00, 0x01, 0x67, 0xAA, 0x00, 0x00, 0x00, 0x01, 0x68};
  auto pos1 = codec::find_start_code(data, 0);
  EXPECT_EQ(pos1, 0u);

  // 첫 번째 NAL 이후에서 다음 start code 탐색
  auto pos2 = codec::find_start_code(data, 3);
  EXPECT_EQ(pos2, 5u);
}

// === start_code_length 테스트 ===

TEST_F(NalUnitParserTest, StartCodeLength_4Byte)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67};
  EXPECT_EQ(codec::start_code_length(data), 4u);
}

TEST_F(NalUnitParserTest, StartCodeLength_3Byte)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x01, 0x67};
  EXPECT_EQ(codec::start_code_length(data), 3u);
}

TEST_F(NalUnitParserTest, StartCodeLength_NotStartCode)
{
  std::vector<uint8_t> data = {0x00, 0x00, 0x02, 0x67};
  EXPECT_EQ(codec::start_code_length(data), 0u);
}

// === parse_nal_units 테스트 ===

TEST_F(NalUnitParserTest, ParseNalUnits_SingleNal)
{
  // SPS NAL: 00 00 00 01 67 42 00 1F
  std::vector<uint8_t> data = {0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00, 0x1F};
  auto units = codec::parse_nal_units(data);
  ASSERT_EQ(units.size(), 1u);
  EXPECT_EQ(units[0].data[0], 0x67); // SPS NAL header
  EXPECT_EQ(units[0].data.size(), 4u);
}

TEST_F(NalUnitParserTest, ParseNalUnits_MultipleNals)
{
  // SPS + PPS
  std::vector<uint8_t> data = {
    0x00,
    0x00,
    0x00,
    0x01,
    0x67,
    0x42,
    0x00,
    0x1F, // SPS
    0x00,
    0x00,
    0x00,
    0x01,
    0x68,
    0xCE,
    0x38,
    0x80 // PPS
  };
  auto units = codec::parse_nal_units(data);
  ASSERT_EQ(units.size(), 2u);
  EXPECT_EQ(units[0].data[0], 0x67); // SPS
  EXPECT_EQ(units[1].data[0], 0x68); // PPS
}

TEST_F(NalUnitParserTest, ParseNalUnits_Mixed3And4ByteStartCodes)
{
  // 4바이트 start code + 3바이트 start code
  std::vector<uint8_t> data = {
    0x00,
    0x00,
    0x00,
    0x01,
    0x67,
    0x42, // SPS (4B SC)
    0x00,
    0x00,
    0x01,
    0x68,
    0xCE // PPS (3B SC)
  };
  auto units = codec::parse_nal_units(data);
  ASSERT_EQ(units.size(), 2u);
  EXPECT_EQ(units[0].data[0], 0x67);
  EXPECT_EQ(units[1].data[0], 0x68);
}

TEST_F(NalUnitParserTest, ParseNalUnits_EmptyInput)
{
  std::vector<uint8_t> data;
  auto units = codec::parse_nal_units(data);
  EXPECT_TRUE(units.empty());
}

TEST_F(NalUnitParserTest, ParseNalUnits_NoStartCode)
{
  std::vector<uint8_t> data = {0x67, 0x42, 0x00, 0x1F};
  auto units = codec::parse_nal_units(data);
  EXPECT_TRUE(units.empty());
}
