// 파일: hmac.h
// 생성일: 2026-02-10
// 설명: HMAC 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <string>

namespace nx::crypto {

enum class HmacAlgorithm
{
  kSha1,
  kSha256,
  kSha512
};

class Hmac
{
public:
  NX_NON_INSTANTIABLE(Hmac);

  static Bytes compute(HmacAlgorithm algorithm, BytesView key, BytesView message);

  static std::string
  compute_hex(HmacAlgorithm algorithm, BytesView key, BytesView message);
};

} // namespace nx::crypto
