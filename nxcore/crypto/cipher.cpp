// 파일: cipher.cpp
// 생성일: 2026-03-17
// 설명: AES-256-GCM 대칭 암호화/복호화 구현 (OpenSSL EVP 기반)

#include "nxcore/crypto/cipher.h"
#include "nxcore/crypto/crypto_error.h"
#include "nxcore/crypto/random.h"

#include <openssl/evp.h>

#include <cstring>

namespace nx::crypto {

nx::expected<Bytes>
Cipher::encrypt(BytesView plaintext, BytesView key)
{
  if (key.size() != kAesKeyLength) {
    return std::unexpected(make_error_code(CryptoError::kInvalidKeyLength));
  }

  // 랜덤 Nonce 생성
  auto nonce = Random::generate_bytes(kAesNonceLength);

  // 결과 버퍼: [Nonce | Ciphertext | Tag]
  Bytes result(kAesNonceLength + plaintext.size() + kAesTagLength);
  std::memcpy(result.data(), nonce.data(), kAesNonceLength);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return std::unexpected(make_error_code(CryptoError::kEncryptFailure));
  }

  int len = 0;
  bool success = true;

  // 초기화
  if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    success = false;
  }

  // Nonce 길이 설정
  if (
    success
    && EVP_CIPHER_CTX_ctrl(
         ctx,
         EVP_CTRL_GCM_SET_IVLEN,
         static_cast<int>(kAesNonceLength),
         nullptr)
         != 1) {
    success = false;
  }

  // 키와 Nonce 설정
  if (
    success && EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce.data()) != 1) {
    success = false;
  }

  // 암호화
  if (success && !plaintext.empty()) {
    if (
      EVP_EncryptUpdate(
        ctx,
        result.data() + kAesNonceLength,
        &len,
        plaintext.data(),
        static_cast<int>(plaintext.size()))
      != 1) {
      success = false;
    }
  }

  // 암호화 완료
  if (success) {
    int final_len = 0;
    if (
      EVP_EncryptFinal_ex(ctx, result.data() + kAesNonceLength + len, &final_len) != 1) {
      success = false;
    }
  }

  // 인증 태그 추출
  if (
    success
    && EVP_CIPHER_CTX_ctrl(
         ctx,
         EVP_CTRL_GCM_GET_TAG,
         static_cast<int>(kAesTagLength),
         result.data() + kAesNonceLength + plaintext.size())
         != 1) {
    success = false;
  }

  EVP_CIPHER_CTX_free(ctx);

  if (!success) {
    return std::unexpected(make_error_code(CryptoError::kEncryptFailure));
  }

  return result;
}

nx::expected<Bytes>
Cipher::decrypt(BytesView ciphertext, BytesView key)
{
  if (key.size() != kAesKeyLength) {
    return std::unexpected(make_error_code(CryptoError::kInvalidKeyLength));
  }

  if (ciphertext.size() < kMinCiphertextLength) {
    return std::unexpected(make_error_code(CryptoError::kInvalidInput));
  }

  // 구성 요소 분리
  const uint8_t* nonce_ptr = ciphertext.data();
  size_t encrypted_len = ciphertext.size() - kAesNonceLength - kAesTagLength;
  const uint8_t* encrypted_ptr = ciphertext.data() + kAesNonceLength;
  const uint8_t* tag_ptr = ciphertext.data() + kAesNonceLength + encrypted_len;

  Bytes plaintext(encrypted_len);

  EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
  if (!ctx) {
    return std::unexpected(make_error_code(CryptoError::kDecryptFailure));
  }

  int len = 0;
  bool success = true;

  // 초기화
  if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
    success = false;
  }

  // Nonce 길이 설정
  if (
    success
    && EVP_CIPHER_CTX_ctrl(
         ctx,
         EVP_CTRL_GCM_SET_IVLEN,
         static_cast<int>(kAesNonceLength),
         nullptr)
         != 1) {
    success = false;
  }

  // 키와 Nonce 설정
  if (success && EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), nonce_ptr) != 1) {
    success = false;
  }

  // 복호화
  if (success && encrypted_len > 0) {
    if (
      EVP_DecryptUpdate(
        ctx,
        plaintext.data(),
        &len,
        encrypted_ptr,
        static_cast<int>(encrypted_len))
      != 1) {
      success = false;
    }
  }

  // 인증 태그 설정
  if (
    success
    && EVP_CIPHER_CTX_ctrl(
         ctx,
         EVP_CTRL_GCM_SET_TAG,
         static_cast<int>(kAesTagLength),
         const_cast<uint8_t*>(tag_ptr))
         != 1) {
    success = false;
  }

  // 복호화 완료 및 인증 태그 검증
  if (success) {
    int final_len = 0;
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &final_len) != 1) {
      success = false;
    }
  }

  EVP_CIPHER_CTX_free(ctx);

  if (!success) {
    return std::unexpected(make_error_code(CryptoError::kDecryptFailure));
  }

  return plaintext;
}

} // namespace nx::crypto
