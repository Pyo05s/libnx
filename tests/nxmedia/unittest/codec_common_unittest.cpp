// 파일: codec_common_unittest.cpp
// 생성일: 2026-04-01
// 설명: 코덱 공통 유틸리티 단위 테스트

#include "nxmedia/codec/common.h"
#include <gtest/gtest.h>

#include <vector>

// ============================================================================
// remove_emulation_prevention 테스트
// ============================================================================

TEST(RemoveEmulationPreventionTest, NoEmulationBytes)
{
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  auto result = nx::media::codec::remove_emulation_prevention(data);

  EXPECT_EQ(result, data);
}

TEST(RemoveEmulationPreventionTest, SingleEmulationByte)
{
  // 00 00 03 → 00 00
  std::vector<uint8_t> data = {0x00, 0x00, 0x03, 0x04};
  auto result = nx::media::codec::remove_emulation_prevention(data);

  std::vector<uint8_t> expected = {0x00, 0x00, 0x04};
  EXPECT_EQ(result, expected);
}

TEST(RemoveEmulationPreventionTest, MultipleEmulationBytes)
{
  // 두 개의 에뮬레이션 방지 바이트
  std::vector<uint8_t> data = {0x00, 0x00, 0x03, 0x00, 0x00, 0x03, 0x01};
  auto result = nx::media::codec::remove_emulation_prevention(data);

  std::vector<uint8_t> expected = {0x00, 0x00, 0x00, 0x00, 0x01};
  EXPECT_EQ(result, expected);
}

TEST(RemoveEmulationPreventionTest, EmptyData)
{
  std::vector<uint8_t> data;
  auto result = nx::media::codec::remove_emulation_prevention(data);

  EXPECT_TRUE(result.empty());
}

TEST(RemoveEmulationPreventionTest, OnlyZeros)
{
  // 00 00 00 → 에뮬레이션 패턴 아님 (0x03이 아니므로)
  std::vector<uint8_t> data = {0x00, 0x00, 0x00};
  auto result = nx::media::codec::remove_emulation_prevention(data);

  EXPECT_EQ(result, data);
}
