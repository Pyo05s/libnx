// 파일: base64.h
// 생성일: 2026-02-10
// 설명: Base64 인코딩/디코딩 기능

#pragma once

#include "nxcore/crypto/crypto_error.h"
#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <expected>
#include <string>
#include <string_view>

namespace nx::crypto {

class Base64
{
public:
  NX_NON_INSTANTIABLE(Base64);

  // 인코딩
  static std::string encode(BytesView data);
  static std::string encode(std::string_view text);

  // 디코딩
  static nx::expected<Bytes> decode(std::string_view encoded);
};

} // namespace nx::crypto
