// 파일: sha256_unittest.cpp
// 생성일: 2026-02-10
// 설명: SHA-256 해시 단위 테스트

#include "nxcore/crypto/sha256.h"

#include <gtest/gtest.h>

TEST(Sha256Test, EmptyString)
{
  std::string hash = nx::crypto::Sha256::hash_hex("");
  EXPECT_EQ(hash, "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
}

TEST(Sha256Test, SimpleString)
{
  std::string hash = nx::crypto::Sha256::hash_hex("abc");
  EXPECT_EQ(hash, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
}

TEST(Sha256Test, IncrementalMatchesSingleShot)
{
  std::string data = "test data";

  auto single_shot = nx::crypto::Sha256::hash(data);

  nx::crypto::Sha256::Context ctx;
  ctx.update(
    nx::crypto::BytesView(
      reinterpret_cast<const uint8_t*>(data.data()),
      data.size()));
  auto incremental = ctx.finalize();

  EXPECT_EQ(single_shot, incremental);
}
