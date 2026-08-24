// 파일: basic_auth_provider.h
// 생성일: 2026-02-10
// 설명: HTTP/RTSP Basic 인증 제공자

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"

#include <nxcore/util/type_util.h>

#include <string>
#include <memory>

namespace nx::net::auth {

// Basic 인증 제공자 (RFC 7617)
class BasicAuthProvider : public AuthProvider
{
public:
  explicit BasicAuthProvider(Credentials credentials);

  AuthScheme scheme() const noexcept override;

  nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const override;

  std::unique_ptr<AuthProvider> clone() const override;

private:
  void generate_cached_header() const;

  Credentials m_credentials;
  mutable std::string m_cached_header; // "Basic base64(user:pass)"
};

} // namespace nx::net::auth