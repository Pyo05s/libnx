// 파일: base64_unittest.cpp
// 생성일: 2026-02-10
// 설명: Base64 인코딩/디코딩 단위 테스트

#include "nxcore/crypto/base64.h"

#include <gtest/gtest.h>

TEST(Base64Test, EncodeEmpty)
{
  std::string encoded = nx::crypto::Base64::encode("");
  EXPECT_EQ(encoded, "");
}

TEST(Base64Test, EncodeSimple)
{
  std::string encoded = nx::crypto::Base64::encode("Hello");
  EXPECT_EQ(encoded, "SGVsbG8=");
}

TEST(Base64Test, DecodeSimple)
{
  auto result = nx::crypto::Base64::decode("SGVsbG8=");
  ASSERT_TRUE(result.has_value());

  std::string decoded(result->begin(), result->end());
  EXPECT_EQ(decoded, "Hello");
}

TEST(Base64Test, RoundTrip)
{
  std::string original = "The quick brown fox jumps over the lazy dog";
  std::string encoded = nx::crypto::Base64::encode(original);
  auto decoded_result = nx::crypto::Base64::decode(encoded);

  ASSERT_TRUE(decoded_result.has_value());
  std::string decoded(decoded_result->begin(), decoded_result->end());
  EXPECT_EQ(decoded, original);
}
