// 파일: auth_provider.cpp
// 생성일: 2026-02-10
// 설명: 인증 제공자 팩토리 구현

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/basic/basic_auth_provider.h"
#include "nxnet/auth/digest/digest_auth_provider.h"
#include "nxnet/auth/bearer/bearer_auth_provider.h"
#include "nxnet/auth/apikey/apikey_auth_provider.h"
#include "nxnet/auth/ws_security/ws_security_provider.h"

namespace nx::net::auth {

std::unique_ptr<AuthProvider>
AuthProviderFactory::create_basic(const Credentials& credentials)
{
  return std::make_unique<BasicAuthProvider>(credentials);
}

std::unique_ptr<AuthProvider>
AuthProviderFactory::create_digest(const Credentials& credentials)
{
  return std::make_unique<DigestAuthProvider>(credentials);
}

std::unique_ptr<AuthProvider>
AuthProviderFactory::create_bearer(std::string token)
{
  auto result = BearerAuthProvider::create(std::move(token));
  if (result.has_value()) {
    return std::move(*result);
  }
  return nullptr;
}

std::unique_ptr<AuthProvider>
AuthProviderFactory::create_api_key(std::string key, std::string header_name)
{
  // 기본적으로 커스텀 헤더 형식 사용
  auto result = ApiKeyAuthProvider::create_with_custom_header(
    std::move(key),
    std::move(header_name));
  if (result.has_value()) {
    return std::move(*result);
  }
  return nullptr;
}

std::unique_ptr<AuthProvider>
AuthProviderFactory::create_ws_security(const Credentials& credentials, bool use_digest)
{
  // use_digest에 따라 생성 방식 선택
  nx::expected<std::unique_ptr<WsSecurityProvider>> result;

  if (use_digest) {
    result = WsSecurityProvider::create_with_digest(credentials);
  }
  else {
    result = WsSecurityProvider::create_with_plain_text(credentials);
  }

  if (result.has_value()) {
    return std::move(*result);
  }
  return nullptr;
}

} // namespace nx::net::auth