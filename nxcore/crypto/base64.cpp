// 파일: base64.cpp
// 생성일: 2026-02-10
// 설명: Base64 인코딩/디코딩 구현 (OpenSSL 기반)

#include "nxcore/crypto/base64.h"

#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <openssl/evp.h>

namespace nx::crypto {

std::string
Base64::encode(BytesView data)
{
  if (data.empty()) {
    return "";
  }

  BIO* bio_mem = BIO_new(BIO_s_mem());
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

  BIO_push(b64, bio_mem);

  BIO_write(b64, data.data(), static_cast<int>(data.size()));
  BIO_flush(b64);

  BUF_MEM* buffer_ptr = nullptr;
  BIO_get_mem_ptr(b64, &buffer_ptr);

  std::string result(buffer_ptr->data, buffer_ptr->length);

  BIO_free_all(b64); // b64와 bio_mem 모두 해제

  return result;
}

std::string
Base64::encode(std::string_view text)
{
  BytesView data(reinterpret_cast<const uint8_t*>(text.data()), text.size());
  return encode(data);
}

nx::expected<Bytes>
Base64::decode(std::string_view encoded)
{
  if (encoded.empty()) {
    return Bytes{};
  }

  BIO* bio_mem = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
  BIO* b64 = BIO_new(BIO_f_base64());
  BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);

  BIO_push(b64, bio_mem);

  Bytes result(encoded.size());
  int decoded_length = BIO_read(b64, result.data(), static_cast<int>(result.size()));

  BIO_free_all(b64); // b64와 bio_mem 모두 해제

  if (decoded_length < 0) {
    return std::unexpected(make_error_code(CryptoError::kInvalidBase64));
  }

  result.resize(decoded_length);
  return result;
}

} // namespace nx::crypto
