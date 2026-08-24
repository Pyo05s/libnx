// 파일: bearer_auth_provider.h
// 생성일: 2026-02-10
// 설명: OAuth 2.0/JWT Bearer Token 인증 제공자

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include <nxcore/util/type_util.h>

#include <string>
#include <memory>
#include <mutex>

namespace nx::net::auth {

// Bearer Token 인증 제공자 (RFC 6750)
class BearerAuthProvider : public AuthProvider
{
public:
  // 검증된 BearerAuthProvider 생성 (빈 토큰 검증)
  static nx::expected<std::unique_ptr<BearerAuthProvider>> create(std::string token);

  AuthScheme scheme() const noexcept override;

  nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const override;

  std::unique_ptr<AuthProvider> clone() const override;

  // 토큰 갱신 (스레드 안전)
  std::error_code update_token(std::string new_token);

  // 현재 토큰 조회 (스레드 안전)
  std::string get_token() const;

private:
  // 생성자는 private (검증 없이 생성)
  explicit BearerAuthProvider(std::string token);

  void generate_cached_header() const;

  // AuthProviderFactory가 생성자에 접근 가능
  friend class AuthProviderFactory;

  mutable std::mutex m_mutex;
  std::string m_token;
  mutable std::string m_cached_header; // "Bearer <token>"
};

} // namespace nx::net::auth
