// 파일: random.h
// 생성일: 2026-02-10
// 설명: 암호학적으로 안전한 난수 생성 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <string>

namespace nx::crypto {

class Random
{
public:
  NX_NON_INSTANTIABLE(Random);

  // 암호학적으로 안전한 난수 생성
  static Bytes generate_bytes(size_t length);

  // UUID v4 생성
  static std::string generate_uuid();

  // 랜덤 문자열 (알파벳+숫자)
  static std::string generate_alphanumeric(size_t length);
};

} // namespace nx::crypto
