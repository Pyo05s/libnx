// 파일: auth_error.cpp
// 생성일: 2026-02-10
// 설명: 네트워크 인증 모듈 오류 코드 구현

#include "nxnet/auth/auth_error.h"

namespace nx::net::auth {

namespace {

// 오류 카테고리 클래스
class AuthErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx.net.auth"; }

  std::string message(int ev) const override
  {
    switch (static_cast<AuthError>(ev)) {
      case AuthError::kSuccess: return "성공";
      case AuthError::kInvalidCredentials: return "잘못된 자격 증명";
      case AuthError::kNoChallenge: return "인증 Challenge 없음";
      case AuthError::kInvalidChallenge: return "잘못된 Challenge 형식";
      case AuthError::kUnsupportedScheme: return "지원하지 않는 인증 스킴";
      case AuthError::kUnsupportedAlgorithm: return "지원하지 않는 알고리즘";
      case AuthError::kMissingParameter: return "필수 매개변수 누락";
      case AuthError::kInvalidParameter: return "잘못된 매개변수";
      case AuthError::kEncodingError: return "인코딩 오류";
      case AuthError::kHashError: return "해시 계산 오류";
      case AuthError::kUnknownError: return "알 수 없는 오류";
      default: return "정의되지 않은 오류";
    }
  }
};

// 싱글톤 카테고리 인스턴스
const AuthErrorCategory&
get_auth_error_category() noexcept
{
  static AuthErrorCategory instance;
  return instance;
}

} // anonymous namespace

std::error_code
make_error_code(AuthError e) noexcept
{
  return {static_cast<int>(e), get_auth_error_category()};
}

} // namespace nx::net::auth