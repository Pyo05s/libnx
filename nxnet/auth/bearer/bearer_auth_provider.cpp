// 파일: bearer_auth_provider.cpp
// 생성일: 2026-02-10
// 설명: OAuth 2.0/JWT Bearer Token 인증 제공자 구현

#include "nxnet/auth/bearer/bearer_auth_provider.h"
#include "nxnet/auth/auth_error.h"

namespace nx::net::auth {

BearerAuthProvider::BearerAuthProvider(std::string token)
    : m_token(std::move(token))
{
  // 캠싱된 헤더 미리 생성
  generate_cached_header();
}

nx::expected<std::unique_ptr<BearerAuthProvider>>
BearerAuthProvider::create(std::string token)
{
  // 토큰 유효성 검증
  if (token.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidParameter));
  }

  // private 생성자 호출
  return std::unique_ptr<BearerAuthProvider>(new BearerAuthProvider(std::move(token)));
}

AuthScheme
BearerAuthProvider::scheme() const noexcept
{
  return AuthScheme::kBearer;
}

nx::expected<std::string>
BearerAuthProvider::generate_authorization_header(const AuthContext&) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // 캐시된 헤더 반환
  return m_cached_header;
}

std::unique_ptr<AuthProvider>
BearerAuthProvider::clone() const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // create() 함수를 사용하여 일관성 유지
  // clone()에서는 이미 검증된 토큰을 복제하므로 실패할 수 없음
  auto result = BearerAuthProvider::create(m_token);
  return std::move(*result);
}

std::error_code
BearerAuthProvider::update_token(std::string new_token)
{
  if (new_token.empty()) {
    return make_error_code(AuthError::kInvalidParameter);
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_token = std::move(new_token);

  // 캠시 무효화 및 재생성
  generate_cached_header();

  return {};
}

std::string
BearerAuthProvider::get_token() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_token;
}

void
BearerAuthProvider::generate_cached_header() const
{
  // RFC 6750: Authorization: Bearer <token>
  m_cached_header = "Bearer " + m_token;
}

} // namespace nx::net::auth
