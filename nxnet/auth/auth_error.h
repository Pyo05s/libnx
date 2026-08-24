// 파일: auth_error.h
// 생성일: 2026-02-10
// 설명: 네트워크 인증 모듈 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx::net::auth {

// 인증 오류 코드
enum class AuthError
{
  kSuccess = 0,
  kInvalidCredentials,
  kNoChallenge,
  kInvalidChallenge,
  kUnsupportedScheme,
  kUnsupportedAlgorithm,
  kMissingParameter,
  kInvalidParameter,
  kEncodingError,
  kHashError,
  kUnknownError
};

// std::error_code 변환
std::error_code make_error_code(AuthError e) noexcept;

} // namespace nx::net::auth

// std::error_code 특수화
namespace std {

template <>
struct is_error_code_enum<nx::net::auth::AuthError> : true_type
{};

} // namespace std