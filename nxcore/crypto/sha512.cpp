// 파일: sha512.cpp
// 생성일: 2026-02-10
// 설명: SHA-512 해시 구현 (OpenSSL 기반)

#include "nxcore/crypto/sha512.h"

#include <openssl/evp.h>

namespace nx::crypto {

namespace {

constexpr size_t kSha512DigestLength = 64;     // 512비트 = 64바이트
constexpr size_t kSha512_256DigestLength = 32; // 256비트 = 32바이트

} // namespace

class Sha512::Context::Impl
{
public:
  Impl()
  {
    m_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(m_ctx, EVP_sha512(), nullptr);
  }

  ~Impl()
  {
    if (m_ctx) {
      EVP_MD_CTX_free(m_ctx);
    }
  }

  void update(BytesView data) { EVP_DigestUpdate(m_ctx, data.data(), data.size()); }

  Bytes finalize()
  {
    Bytes result(kSha512DigestLength);
    unsigned int length = 0;
    EVP_DigestFinal_ex(m_ctx, result.data(), &length);
    return result;
  }

  Bytes finalize_256()
  {
    Bytes full_hash = finalize();
    Bytes result(kSha512_256DigestLength);
    std::copy_n(full_hash.begin(), kSha512_256DigestLength, result.begin());
    return result;
  }

private:
  EVP_MD_CTX* m_ctx = nullptr;
};

Sha512::Context::Context()
    : m_impl(std::make_unique<Impl>())
{}

Sha512::Context::~Context() = default;

void
Sha512::Context::update(BytesView data)
{
  m_impl->update(data);
}

Bytes
Sha512::Context::finalize()
{
  return m_impl->finalize();
}

Bytes
Sha512::Context::finalize_256()
{
  return m_impl->finalize_256();
}

Bytes
Sha512::hash(BytesView data)
{
  Context ctx;
  ctx.update(data);
  return ctx.finalize();
}

Bytes
Sha512::hash(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return hash(data);
}

std::string
Sha512::hash_hex(BytesView data)
{
  return bytes_to_hex(hash(data));
}

std::string
Sha512::hash_hex(std::string_view text)
{
  return bytes_to_hex(hash(text));
}

Bytes
Sha512::hash_256(BytesView data)
{
  Context ctx;
  ctx.update(data);
  return ctx.finalize_256();
}

Bytes
Sha512::hash_256(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return hash_256(data);
}

std::string
Sha512::hash_hex_256(BytesView data)
{
  return bytes_to_hex(hash_256(data));
}

std::string
Sha512::hash_hex_256(std::string_view text)
{
  return bytes_to_hex(hash_256(text));
}

} // namespace nx::crypto
