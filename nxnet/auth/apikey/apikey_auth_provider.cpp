// 파일: apikey_auth_provider.cpp
// 생성일: 2026-02-10
// 설명: REST API Key 인증 제공자 구현

#include "nxnet/auth/apikey/apikey_auth_provider.h"
#include "nxnet/auth/auth_error.h"

namespace nx::net::auth {

ApiKeyAuthProvider::ApiKeyAuthProvider(
  std::string key, std::string header_name, bool use_auth_header)
    : m_key(std::move(key))
    , m_header_name(std::move(header_name))
    , m_use_auth_header(use_auth_header)
{
  // 캐싱된 헤더 미리 생성
  generate_cached_header();
}

nx::expected<std::unique_ptr<ApiKeyAuthProvider>>
ApiKeyAuthProvider::create_with_custom_header(std::string key, std::string header_name)
{
  // API Key 유효성 검증
  if (key.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidParameter));
  }

  // 헤더 이름 유효성 검증
  if (header_name.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidParameter));
  }

  // private 생성자 호출 - 커스텀 헤더 사용
  return std::unique_ptr<ApiKeyAuthProvider>(
    new ApiKeyAuthProvider(std::move(key), std::move(header_name), false));
}

nx::expected<std::unique_ptr<ApiKeyAuthProvider>>
ApiKeyAuthProvider::create_with_auth_header(std::string key, std::string scheme_name)
{
  // API Key 유효성 검증
  if (key.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidParameter));
  }

  // 스킴 이름 유효성 검증
  if (scheme_name.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidParameter));
  }

  // private 생성자 호출 - Authorization 헤더 사용
  return std::unique_ptr<ApiKeyAuthProvider>(
    new ApiKeyAuthProvider(std::move(key), std::move(scheme_name), true));
}

AuthScheme
ApiKeyAuthProvider::scheme() const noexcept
{
  return AuthScheme::kApiKey;
}

nx::expected<std::string>
ApiKeyAuthProvider::generate_authorization_header(const AuthContext&) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // 캐시된 헤더 반환
  // 주의: 커스텀 헤더 사용 시 "X-API-Key: value" 형태로 반환되지만,
  // HTTP 클라이언트는 이를 적절히 분리하여 사용해야 함
  return m_cached_header;
}

nx::expected<boost::beast::http::fields>
ApiKeyAuthProvider::generate_headers(const AuthContext&) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_use_auth_header) {
    // Authorization 헤더 형식: "Authorization: <scheme> <key>"
    // 예: "Authorization: ApiKey abc123"
    boost::beast::http::fields headers;
    headers.set(boost::beast::http::field::authorization, m_header_name + " " + m_key);
    return headers;
  }
  else {
    // 커스텀 헤더 형식: "<header_name>: <key>"
    // 예: "X-API-Key: abc123"
    boost::beast::http::fields headers;
    headers.set(m_header_name, m_key);
    return headers;
  }
}

std::unique_ptr<AuthProvider>
ApiKeyAuthProvider::clone() const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // create 함수를 사용하여 일관성 유지
  if (m_use_auth_header) {
    auto result = ApiKeyAuthProvider::create_with_auth_header(m_key, m_header_name);
    return std::move(*result);
  }
  else {
    auto result = ApiKeyAuthProvider::create_with_custom_header(m_key, m_header_name);
    return std::move(*result);
  }
}

std::error_code
ApiKeyAuthProvider::update_key(std::string new_key)
{
  if (new_key.empty()) {
    return make_error_code(AuthError::kInvalidParameter);
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_key = std::move(new_key);

  // 캐시 무효화 및 재생성
  generate_cached_header();

  return {};
}

std::string
ApiKeyAuthProvider::get_key() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_key;
}

std::string
ApiKeyAuthProvider::get_header_name() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_header_name;
}

bool
ApiKeyAuthProvider::uses_auth_header() const noexcept
{
  return m_use_auth_header;
}

void
ApiKeyAuthProvider::generate_cached_header() const
{
  if (m_use_auth_header) {
    // Authorization 헤더 형식: "Authorization: <scheme> <key>"
    // 예: "Authorization: ApiKey abc123"
    m_cached_header = m_header_name + " " + m_key;
  }
  else {
    // 커스텀 헤더 형식: "<header_name>: <key>"
    // 예: "X-API-Key: abc123"
    // 주의: 실제 HTTP 요청에서는 헤더 이름과 값을 분리해야 함
    m_cached_header = m_header_name + ": " + m_key;
  }
}

} // namespace nx::net::auth
