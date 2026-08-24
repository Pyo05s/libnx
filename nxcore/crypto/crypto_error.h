// 파일: crypto_error.h
// 생성일: 2026-02-10
// 설명: 암호화 모듈의 오류 코드 정의

#pragma once

#include <system_error>

namespace nx::crypto {

enum class CryptoError
{
  kSuccess = 0,
  kInvalidInput,     // 잘못된 입력 데이터
  kInvalidBase64,    // Base64 디코딩 실패
  kHashFailure,      // 해시 계산 실패
  kHmacFailure,      // HMAC 계산 실패
  kRandomFailure,    // 난수 생성 실패
  kEncryptFailure,   // 암호화 실패
  kDecryptFailure,   // 복호화 실패 (인증 태그 불일치 포함)
  kInvalidKeyLength, // 유효하지 않은 키 길이
  kInternalError     // 내부 오류
};

std::error_code make_error_code(CryptoError e);

} // namespace nx::crypto

namespace std {
template <>
struct is_error_code_enum<nx::crypto::CryptoError> : true_type
{};
} // namespace std
