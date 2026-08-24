// 파일: md5.cpp
// 생성일: 2026-02-10
// 설명: MD5 해시 구현 (OpenSSL 기반)

#include "nxcore/crypto/md5.h"

#include <openssl/evp.h>

#include <array>

namespace nx::crypto {

namespace {

constexpr size_t kMd5DigestLength = 16;

} // namespace

class Md5::Context::Impl
{
public:
  Impl()
  {
    m_ctx = EVP_MD_CTX_new();
    EVP_DigestInit_ex(m_ctx, EVP_md5(), nullptr);
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
    Bytes result(kMd5DigestLength);
    unsigned int length = 0;
    EVP_DigestFinal_ex(m_ctx, result.data(), &length);
    return result;
  }

private:
  EVP_MD_CTX* m_ctx = nullptr;
};

Md5::Context::Context()
    : m_impl(std::make_unique<Impl>())
{}

Md5::Context::~Context() = default;

void
Md5::Context::update(BytesView data)
{
  m_impl->update(data);
}

Bytes
Md5::Context::finalize()
{
  return m_impl->finalize();
}

Bytes
Md5::hash(BytesView data)
{
  Context ctx;
  ctx.update(data);
  return ctx.finalize();
}

Bytes
Md5::hash(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return hash(data);
}

std::string
Md5::hash_hex(BytesView data)
{
  return bytes_to_hex(hash(data));
}

std::string
Md5::hash_hex(std::string_view text)
{
  return bytes_to_hex(hash(text));
}

} // namespace nx::crypto
