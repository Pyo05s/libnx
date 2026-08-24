// 파일: ws_security_provider.h
// 생성일: 2026-02-10
// 설명: WS-Security(ONVIF) 인증 제공자

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include "nxnet/auth/ws_security/ws_security_types.h"

#include <nxcore/util/type_util.h>

#include <string>
#include <memory>
#include <mutex>

namespace nx::net::auth {

// WS-Security 인증 제공자 (OASIS WS-Security UsernameToken Profile)
class WsSecurityProvider : public AuthProvider
{
public:
  // PasswordDigest 형식의 WS-Security 인증 생성
  // algorithm: 해시 알고리즘 (기본값: SHA-1)
  static nx::expected<std::unique_ptr<WsSecurityProvider>> create_with_digest(
    const Credentials& credentials,
    ws_security::HashAlgorithm algorithm = ws_security::HashAlgorithm::kSha1);

  // PasswordText 형식의 WS-Security 인증 생성 (보안상 권장하지 않음)
  static nx::expected<std::unique_ptr<WsSecurityProvider>>
  create_with_plain_text(const Credentials& credentials);

  AuthScheme scheme() const noexcept override;

  nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const override;

  // SOAP Security 헤더 생성 (ONVIF/SOAP 전용)
  nx::expected<std::string> generate_soap_security_header() const override;

  std::unique_ptr<AuthProvider> clone() const override;

  // 자격 증명 갱신 (스레드 안전)
  std::error_code update_credentials(const Credentials& new_credentials);

  // 현재 자격 증명 조회 (스레드 안전)
  Credentials get_credentials() const;

  // Digest 모드 여부 확인
  bool uses_digest() const noexcept;

  // 해시 알고리즘 조회
  ws_security::HashAlgorithm get_algorithm() const noexcept;

private:
  // 생성자는 private (검증 없이 생성 방지)
  WsSecurityProvider(
    const Credentials& credentials,
    bool use_digest,
    ws_security::HashAlgorithm algorithm);

  // AuthProviderFactory가 생성자에 접근 가능
  friend class AuthProviderFactory;

  mutable std::mutex m_mutex;
  Credentials m_credentials;
  bool m_use_digest;                      // true: PasswordDigest, false: PasswordText
  ws_security::HashAlgorithm m_algorithm; // 해시 알고리즘
};

} // namespace nx::net::auth
