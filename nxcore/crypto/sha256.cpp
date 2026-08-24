// 파일: sha256.cpp
// 생성일: 2026-02-10
// 설명: SHA-256 해시 구현 (OpenSSL 기반)

#include "nxcore/crypto/sha256.h"

#include <openssl/evp.h>

namespace nx::crypto {

namespace {

constexpr size_t kSha256DigestLength = 32;

} // namespace

class Sha256::Context::Impl
{
public:
  Impl()
  {
    m_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(m_ctx, EVP_sha256(), nullptr);
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
    Bytes result(kSha256DigestLength);
    unsigned int length = 0;
    EVP_DigestFinal_ex(m_ctx, result.data(), &length);
    return result;
  }

private:
  EVP_MD_CTX* m_ctx = nullptr;
};

Sha256::Context::Context()
    : m_impl(std::make_unique<Impl>())
{}

Sha256::Context::~Context() = default;

void
Sha256::Context::update(BytesView data)
{
  m_impl->update(data);
}

Bytes
Sha256::Context::finalize()
{
  return m_impl->finalize();
}

Bytes
Sha256::hash(BytesView data)
{
  Context ctx;
  ctx.update(data);
  return ctx.finalize();
}

Bytes
Sha256::hash(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return hash(data);
}

std::string
Sha256::hash_hex(BytesView data)
{
  return bytes_to_hex(hash(data));
}

std::string
Sha256::hash_hex(std::string_view text)
{
  return bytes_to_hex(hash(text));
}

} // namespace nx::crypto
