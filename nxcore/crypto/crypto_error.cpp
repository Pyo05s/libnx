// 파일: crypto_error.cpp
// 생성일: 2026-02-10
// 설명: 암호화 모듈 오류 코드 구현

#include "nxcore/crypto/crypto_error.h"

namespace {

class CryptoErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::crypto"; }

  std::string message(int ev) const override
  {
    using nx::crypto::CryptoError;
    switch (static_cast<CryptoError>(ev)) {
      case CryptoError::kSuccess: return "Success";
      case CryptoError::kInvalidInput: return "Invalid input data";
      case CryptoError::kInvalidBase64: return "Invalid Base64 string";
      case CryptoError::kHashFailure: return "Hash calculation failed";
      case CryptoError::kHmacFailure: return "HMAC calculation failed";
      case CryptoError::kRandomFailure: return "Random generation failed";
      case CryptoError::kEncryptFailure: return "Encryption failed";
      case CryptoError::kDecryptFailure: return "Decryption failed";
      case CryptoError::kInvalidKeyLength: return "Invalid key length";
      case CryptoError::kInternalError: return "Internal error";
      default: return "Unknown error";
    }
  }
};

const CryptoErrorCategory&
crypto_error_category()
{
  static CryptoErrorCategory instance;
  return instance;
}

} // namespace

namespace nx::crypto {

std::error_code
make_error_code(CryptoError e)
{
  return {static_cast<int>(e), crypto_error_category()};
}

} // namespace nx::crypto
