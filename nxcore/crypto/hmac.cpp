// 파일: hmac.cpp
// 생성일: 2026-02-10
// 설명: HMAC 구현 (OpenSSL 기반)

#include "nxcore/crypto/hmac.h"

#include <openssl/evp.h>
#include <openssl/hmac.h>

namespace nx::crypto {

namespace {

const EVP_MD*
get_evp_md(HmacAlgorithm algorithm)
{
  switch (algorithm) {
    case HmacAlgorithm::kSha1: return EVP_sha1();
    case HmacAlgorithm::kSha256: return EVP_sha256();
    case HmacAlgorithm::kSha512: return EVP_sha512();
    default: return nullptr;
  }
}

} // namespace

Bytes
Hmac::compute(HmacAlgorithm algorithm, BytesView key, BytesView message)
{
  const EVP_MD* md = get_evp_md(algorithm);
  if (!md) {
    return {};
  }

  unsigned int result_length = EVP_MD_size(md);
  Bytes result(result_length);

  ::HMAC(
    md,
    key.data(),
    static_cast<int>(key.size()),
    message.data(),
    message.size(),
    result.data(),
    &result_length);

  return result;
}

std::string
Hmac::compute_hex(HmacAlgorithm algorithm, BytesView key, BytesView message)
{
  return bytes_to_hex(compute(algorithm, key, message));
}

} // namespace nx::crypto
