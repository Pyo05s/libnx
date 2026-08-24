// 파일: basic_auth_provider.cpp
// 생성일: 2026-02-10
// 설명: HTTP/RTSP Basic 인증 제공자 구현

#include "nxnet/auth/basic/basic_auth_provider.h"
#include "nxnet/auth/auth_error.h"
#include "nxcore/crypto/base64.h"

namespace nx::net::auth {

BasicAuthProvider::BasicAuthProvider(Credentials credentials)
    : m_credentials(std::move(credentials))
{
  // 자격 증명 유효성 검증
  if (m_credentials.username.empty()) {
    // 빈 사용자명은 허용하지 않음 (일부 서버에서 문제 발생)
    m_credentials.username = " ";
  }
}

AuthScheme
BasicAuthProvider::scheme() const noexcept
{
  return AuthScheme::kBasic;
}

nx::expected<std::string>
BasicAuthProvider::generate_authorization_header(const AuthContext&) const
{
  // 캐시된 헤더가 없으면 생성
  if (m_cached_header.empty()) {
    generate_cached_header();
  }

  return m_cached_header;
}

std::unique_ptr<AuthProvider>
BasicAuthProvider::clone() const
{
  return std::make_unique<BasicAuthProvider>(m_credentials);
}

void
BasicAuthProvider::generate_cached_header() const
{
  // RFC 7617: credentials = username ":" password
  std::string credentials_str = m_credentials.username + ":" + m_credentials.password;

  // Base64 인코딩
  std::string encoded = nx::crypto::Base64::encode(credentials_str);

  // Authorization 헤더 형식: "Basic " + base64(credentials)
  m_cached_header = "Basic " + encoded;
}

} // namespace nx::net::auth