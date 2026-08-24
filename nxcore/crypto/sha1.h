// 파일: sha1.h
// 생성일: 2026-02-10
// 설명: SHA-1 해시 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <memory>
#include <string>
#include <string_view>

namespace nx::crypto {

class Sha1
{
public:
  NX_NON_INSTANTIABLE(Sha1);

  static Bytes hash(BytesView data);
  static Bytes hash(std::string_view text);
  static std::string hash_hex(BytesView data);
  static std::string hash_hex(std::string_view text);

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
