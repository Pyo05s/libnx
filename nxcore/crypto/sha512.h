// 파일: sha512.h
// 생성일: 2026-02-10
// 설명: SHA-512 해시 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <memory>
#include <string>
#include <string_view>

namespace nx::crypto {

class Sha512
{
public:
  NX_NON_INSTANTIABLE(Sha512);

  static Bytes hash(BytesView data);
  static Bytes hash(std::string_view text);
  static std::string hash_hex(BytesView data);
  static std::string hash_hex(std::string_view text);

  // SHA-512의 앞쪽 256비트만 반환 (RFC 7616)
  static Bytes hash_256(BytesView data);
  static Bytes hash_256(std::string_view text);
  static std::string hash_hex_256(BytesView data);
  static std::string hash_hex_256(std::string_view text);

  class Context
  {
  public:
    Context();
    ~Context();

    NX_NON_COPYABLE_AND_MOVABLE(Context);

    void update(BytesView data);
    Bytes finalize();
    Bytes finalize_256(); // 앞쪽 256비트만 반환

  private:
    class Impl;
    std::unique_ptr<Impl> m_impl;
  };
};

} // namespace nx::crypto
