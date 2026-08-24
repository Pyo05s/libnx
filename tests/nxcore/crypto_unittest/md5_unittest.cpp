// 파일: md5_unittest.cpp
// 생성일: 2026-02-10
// 설명: MD5 해시 단위 테스트

#include "nxcore/crypto/md5.h"

#include <gtest/gtest.h>

TEST(Md5Test, EmptyString)
{
  std::string hash = nx::crypto::Md5::hash_hex("");
  EXPECT_EQ(hash, "d41d8cd98f00b204e9800998ecf8427e");
}

TEST(Md5Test, SimpleString)
{
  std::string hash = nx::crypto::Md5::hash_hex("abc");
  EXPECT_EQ(hash, "900150983cd24fb0d6963f7d28e17f72");
}

TEST(Md5Test, IncrementalMatchesSingleShot)
{
  std::string data = "The quick brown fox jumps over the lazy dog";

  auto single_shot = nx::crypto::Md5::hash(data);

  nx::crypto::Md5::Context ctx;
  ctx.update(
    nx::crypto::BytesView(reinterpret_cast<const uint8_t*>(data.data()), 10));
  ctx.update(
    nx::crypto::BytesView(
      reinterpret_cast<const uint8_t*>(data.data() + 10),
      data.size() - 10));
  auto incremental = ctx.finalize();

  EXPECT_EQ(single_shot, incremental);
}
