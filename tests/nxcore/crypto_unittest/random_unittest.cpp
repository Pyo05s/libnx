// 파일: random_unittest.cpp
// 생성일: 2026-02-10
// 설명: 난수 생성 단위 테스트

#include "nxcore/crypto/random.h"

#include <gtest/gtest.h>
#include <regex>

TEST(RandomTest, GenerateBytesLength)
{
  auto bytes = nx::crypto::Random::generate_bytes(16);
  EXPECT_EQ(bytes.size(), 16);
}

TEST(RandomTest, GenerateBytesNotZero)
{
  auto bytes = nx::crypto::Random::generate_bytes(32);

  // 모든 바이트가 0일 확률은 극히 낮음
  bool has_non_zero = false;
  for (uint8_t byte : bytes) {
    if (byte != 0) {
      has_non_zero = true;
      break;
    }
  }
  EXPECT_TRUE(has_non_zero);
}

TEST(RandomTest, UuidFormat)
{
  std::string uuid = nx::crypto::Random::generate_uuid();

  // UUID v4 형식: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  std::regex uuid_pattern(
    "[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}");

  EXPECT_TRUE(std::regex_match(uuid, uuid_pattern));
}

TEST(RandomTest, AlphanumericLength)
{
  std::string random_str = nx::crypto::Random::generate_alphanumeric(32);
  EXPECT_EQ(random_str.length(), 32);
}

TEST(RandomTest, AlphanumericCharacters)
{
  std::string random_str = nx::crypto::Random::generate_alphanumeric(100);

  // 모든 문자가 알파벳이거나 숫자여야 함
  for (char c : random_str) {
    EXPECT_TRUE(std::isalnum(static_cast<unsigned char>(c)));
  }
}
