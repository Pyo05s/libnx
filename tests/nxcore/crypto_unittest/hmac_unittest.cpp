// 파일: hmac_unittest.cpp
// 생성일: 2026-02-10
// 설명: HMAC 단위 테스트

#include "nxcore/crypto/hmac.h"

#include <gtest/gtest.h>

TEST(HmacTest, Sha256Basic)
{
  std::string key = "key";
  std::string message = "The quick brown fox jumps over the lazy dog";

  nx::crypto::BytesView key_view(
    reinterpret_cast<const uint8_t*>(key.data()),
    key.size());
  nx::crypto::BytesView msg_view(
    reinterpret_cast<const uint8_t*>(message.data()),
    message.size());

  std::string hmac_hex = nx::crypto::Hmac::compute_hex(
    nx::crypto::HmacAlgorithm::kSha256,
    key_view,
    msg_view);

  EXPECT_EQ(
    hmac_hex,
    "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8");
}

TEST(HmacTest, Sha1Basic)
{
  std::string key = "key";
  std::string message = "message";

  nx::crypto::BytesView key_view(
    reinterpret_cast<const uint8_t*>(key.data()),
    key.size());
  nx::crypto::BytesView msg_view(
    reinterpret_cast<const uint8_t*>(message.data()),
    message.size());

  auto result
    = nx::crypto::Hmac::compute(nx::crypto::HmacAlgorithm::kSha1, key_view, msg_view);

  EXPECT_FALSE(result.empty());
  EXPECT_EQ(result.size(), 20); // SHA-1은 20바이트
}
