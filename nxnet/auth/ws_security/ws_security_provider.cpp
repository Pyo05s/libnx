// 파일: ws_security_provider.cpp
// 생성일: 2026-02-10
// 설명: WS-Security(ONVIF) 인증 제공자 구현

#include "nxnet/auth/ws_security/ws_security_provider.h"
#include "nxnet/auth/ws_security/username_token.h"
#include "nxnet/auth/auth_error.h"

namespace nx::net::auth {

WsSecurityProvider::WsSecurityProvider(
  const Credentials& credentials, bool use_digest, ws_security::HashAlgorithm algorithm)
    : m_credentials(credentials)
    , m_use_digest(use_digest)
    , m_algorithm(algorithm)
{}

nx::expected<std::unique_ptr<WsSecurityProvider>>
WsSecurityProvider::create_with_digest(
  const Credentials& credentials, ws_security::HashAlgorithm algorithm)
{
  // 자격 증명 유효성 검증
  if (credentials.username.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidCredentials));
  }

  // private 생성자 호출 - PasswordDigest 모드
  return std::unique_ptr<WsSecurityProvider>(
    new WsSecurityProvider(credentials, true, algorithm));
}

nx::expected<std::unique_ptr<WsSecurityProvider>>
WsSecurityProvider::create_with_plain_text(const Credentials& credentials)
{
  // 자격 증명 유효성 검증
  if (credentials.username.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidCredentials));
  }

  // private 생성자 호출 - PasswordText 모드
  return std::unique_ptr<WsSecurityProvider>(
    new WsSecurityProvider(credentials, false, ws_security::HashAlgorithm::kSha1));
}

AuthScheme
WsSecurityProvider::scheme() const noexcept
{
  return AuthScheme::kWsSecurity;
}

nx::expected<std::string>
WsSecurityProvider::generate_authorization_header(const AuthContext&) const
{
  // WS-Security는 HTTP Authorization 헤더를 사용하지 않음
  // SOAP 메시지의 Security 헤더를 사용
  return std::unexpected(make_error_code(std::errc::not_supported));
}

nx::expected<std::string>
WsSecurityProvider::generate_soap_security_header() const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // UsernameToken 생성
  nx::expected<ws_security::UsernameToken> token_result;

  if (m_use_digest) {
    token_result
      = ws_security::UsernameTokenBuilder::create_with_digest(m_credentials, m_algorithm);
  }
  else {
    token_result
      = ws_security::UsernameTokenBuilder::create_with_plain_text(m_credentials);
  }

  if (!token_result.has_value()) {
    return std::unexpected(token_result.error());
  }

  // SOAP Security 헤더 XML 생성
  std::string soap_header
    = ws_security::UsernameTokenBuilder::generate_soap_header_xml(*token_result);

  return soap_header;
}

std::unique_ptr<AuthProvider>
WsSecurityProvider::clone() const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // create 함수를 사용하여 일관성 유지
  if (m_use_digest) {
    auto result = WsSecurityProvider::create_with_digest(m_credentials, m_algorithm);
    return std::move(*result);
  }
  else {
    auto result = WsSecurityProvider::create_with_plain_text(m_credentials);
    return std::move(*result);
  }
}

std::error_code
WsSecurityProvider::update_credentials(const Credentials& new_credentials)
{
  if (new_credentials.username.empty()) {
    return make_error_code(AuthError::kInvalidCredentials);
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_credentials = new_credentials;

  return {};
}

Credentials
WsSecurityProvider::get_credentials() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_credentials;
}

bool
WsSecurityProvider::uses_digest() const noexcept
{
  return m_use_digest;
}

ws_security::HashAlgorithm
WsSecurityProvider::get_algorithm() const noexcept
{
  return m_algorithm;
}

} // namespace nx::net::auth
