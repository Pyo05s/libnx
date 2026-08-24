// 파일: sha1.cpp
// 생성일: 2026-02-10
// 설명: SHA-1 해시 구현 (OpenSSL 기반)

#include "nxcore/crypto/sha1.h"

#include <openssl/evp.h>

namespace nx::crypto {

namespace {

constexpr size_t kSha1DigestLength = 20;

} // namespace

class Sha1::Context::Impl
{
public:
  Impl()
  {
    m_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(m_ctx, EVP_sha1(), nullptr);
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
    Bytes result(kSha1DigestLength);
    unsigned int length = 0;
    EVP_DigestFinal_ex(m_ctx, result.data(), &length);
    return result;
  }

private:
  EVP_MD_CTX* m_ctx = nullptr;
};

Sha1::Context::Context()
    : m_impl(std::make_unique<Impl>())
{}

Sha1::Context::~Context() = default;

void
Sha1::Context::update(BytesView data)
{
  m_impl->update(data);
}

Bytes
Sha1::Context::finalize()
{
  return m_impl->finalize();
}

Bytes
Sha1::hash(BytesView data)
{
  Context ctx;
  ctx.update(data);
  return ctx.finalize();
}

Bytes
Sha1::hash(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return hash(data);
}

std::string
Sha1::hash_hex(BytesView data)
{
  return bytes_to_hex(hash(data));
}

std::string
Sha1::hash_hex(std::string_view text)
{
  return bytes_to_hex(hash(text));
}

} // namespace nx::crypto
