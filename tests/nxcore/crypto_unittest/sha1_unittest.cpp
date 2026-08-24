// 파일: sha1_unittest.cpp
// 생성일: 2026-02-10
// 설명: SHA-1 해시 단위 테스트

#include "nxcore/crypto/sha1.h"

#include <gtest/gtest.h>

TEST(Sha1Test, EmptyString)
{
  std::string hash = nx::crypto::Sha1::hash_hex("");
  EXPECT_EQ(hash, "da39a3ee5e6b4b0d3255bfef95601890afd80709");
}

TEST(Sha1Test, SimpleString)
{
  std::string hash = nx::crypto::Sha1::hash_hex("abc");
  EXPECT_EQ(hash, "a9993e364706816aba3e25717850c26c9cd0d89d");
}

TEST(Sha1Test, IncrementalMatchesSingleShot)
{
  std::string data = "test data";

  auto single_shot = nx::crypto::Sha1::hash(data);

  nx::crypto::Sha1::Context ctx;
  ctx.update(
    nx::crypto::BytesView(
      reinterpret_cast<const uint8_t*>(data.data()),
      data.size()));
  auto incremental = ctx.finalize();

  EXPECT_EQ(single_shot, incremental);
}
