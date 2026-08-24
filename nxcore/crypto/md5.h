// 파일: md5.h
// 생성일: 2026-02-10
// 설명: MD5 해시 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <memory>
#include <string>
#include <string_view>

namespace nx::crypto {

class Md5
{
public:
  NX_NON_INSTANTIABLE(Md5);

  // 한 번에 해시 계산
  static Bytes hash(BytesView data);
  static Bytes hash(std::string_view text);

  // 16진수 문자열로 반환
  static std::string hash_hex(BytesView data);
  static std::string hash_hex(std::string_view text);

  // 증분 해시 (대용량 데이터)
  class Context
  {
  public:
    Context();
    ~Context();

    NX_NON_COPYABLE_AND_MOVABLE(Context);

    void update(BytesView data);
    Bytes finalize();

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
  };
};

} // namespace nx::crypto
