// 파일: bit_util_unittest.cpp
// 생성일: 2026-04-01
// 설명: BitReader 단위 테스트

#include "nxcore/util/bit_util.h"
#include <gtest/gtest.h>

#include <array>
#include <vector>

// ============================================================================
// BitReader 테스트
// ============================================================================

TEST(BitReaderTest, ReadBits_SingleByte)
{
  // 0xA5 = 1010 0101
  std::array<uint8_t, 1> data = {0xA5};
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_bits(4), 0x0A); // 1010
  EXPECT_EQ(reader.read_bits(4), 0x05); // 0101
}

TEST(BitReaderTest, ReadBits_CrossByte)
{
  // 0xFF 0x00 = 11111111 00000000
  std::array<uint8_t, 2> data = {0xFF, 0x00};
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_bits(4), 0x0F); // 1111
  EXPECT_EQ(reader.read_bits(8), 0xF0); // 1111 0000 (바이트 경계 횡단)
  EXPECT_EQ(reader.read_bits(4), 0x00); // 0000
}

TEST(BitReaderTest, ReadBit)
{
  // 0xC0 = 1100 0000
  std::array<uint8_t, 1> data = {0xC0};
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_bit(), 1u);
  EXPECT_EQ(reader.read_bit(), 1u);
  EXPECT_EQ(reader.read_bit(), 0u);
  EXPECT_EQ(reader.read_bit(), 0u);
}

TEST(BitReaderTest, ReadUe_Zero)
{
  // ue(0) = 1 (단일 비트 '1')
  std::array<uint8_t, 1> data = {0x80}; // 1000 0000
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_ue(), 0u);
}

TEST(BitReaderTest, ReadUe_Values)
{
  // ue(1) = 010 → 0x40 = 0100 0000
  // ue(2) = 011 → 이어서 0110 ...
  // 전체: 010 011 00 → 0x4C
  std::array<uint8_t, 1> data = {0x4C}; // 0100 1100
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_ue(), 1u); // 010
  EXPECT_EQ(reader.read_ue(), 2u); // 011
}

TEST(BitReaderTest, ReadSe_Positive)
{
  // se(+1) = ue(1) = 010
  std::array<uint8_t, 1> data = {0x40}; // 0100 0000
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_se(), 1);
}

TEST(BitReaderTest, ReadSe_Negative)
{
  // se(-1) = ue(2) = 011
  std::array<uint8_t, 1> data = {0x60}; // 0110 0000
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_se(), -1);
}

TEST(BitReaderTest, SkipBits)
{
  // 0xAB = 1010 1011
  std::array<uint8_t, 1> data = {0xAB};
  nx::BitReader reader(data);

  reader.skip_bits(4);
  EXPECT_EQ(reader.read_bits(4), 0x0B); // 1011
}

TEST(BitReaderTest, HasMoreData)
{
  std::array<uint8_t, 1> data = {0xFF};
  nx::BitReader reader(data);

  EXPECT_TRUE(reader.has_more_data());
  reader.read_bits(8);
  EXPECT_FALSE(reader.has_more_data());
}

TEST(BitReaderTest, ReadBeyondData)
{
  std::array<uint8_t, 1> data = {0xFF};
  nx::BitReader reader(data);

  reader.read_bits(8);
  // 데이터 범위를 초과해도 크래시하지 않음
  EXPECT_EQ(reader.read_bits(8), 0u);
}

TEST(BitReaderTest, ReadBits_32bit)
{
  // 4바이트 전체 읽기
  std::array<uint8_t, 4> data = {0xDE, 0xAD, 0xBE, 0xEF};
  nx::BitReader reader(data);

  EXPECT_EQ(reader.read_bits(32), 0xDEADBEEFu);
}
