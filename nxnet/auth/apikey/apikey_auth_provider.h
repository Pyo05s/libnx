// 파일: apikey_auth_provider.h
// 생성일: 2026-02-10
// 설명: REST API Key 인증 제공자

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"

#include <nxcore/util/type_util.h>

#include <memory>
#include <mutex>
#include <string>

namespace nx::net::auth {

// API Key 인증 제공자
class ApiKeyAuthProvider : public AuthProvider
{
public:
  // 커스텀 헤더 형식으로 API Key 인증 생성 (예: "X-API-Key: <key>")
  static nx::expected<std::unique_ptr<ApiKeyAuthProvider>>
  create_with_custom_header(std::string key, std::string header_name = "X-API-Key");

  // Authorization 헤더 형식으로 API Key 인증 생성 (예: "Authorization: ApiKey
  // <key>")
  static nx::expected<std::unique_ptr<ApiKeyAuthProvider>>
  create_with_auth_header(std::string key, std::string scheme_name = "ApiKey");

  AuthScheme scheme() const noexcept override;

  nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const override;

  nx::expected<boost::beast::http::fields>
  generate_headers(const AuthContext& context) const override;

  std::unique_ptr<AuthProvider> clone() const override;

  // API Key 갱신 (스레드 안전)
  std::error_code update_key(std::string new_key);

  // 현재 API Key 조회 (스레드 안전)
  std::string get_key() const;

  // 헤더 이름 조회
  std::string get_header_name() const;

  // Authorization 헤더 사용 여부
  bool uses_auth_header() const noexcept;

private:
  // 생성자는 private (검증 없이 생성 방지)
  ApiKeyAuthProvider(std::string key, std::string header_name, bool use_auth_header);

  void generate_cached_header() const;

  // AuthProviderFactory가 생성자에 접근 가능
  friend class AuthProviderFactory;

  mutable std::mutex m_mutex;
  std::string m_key;
  std::string m_header_name; // "X-API-Key" 또는 "ApiKey" (Authorization 스킴)
  bool m_use_auth_header;    // true면 Authorization 헤더, false면 커스텀 헤더
  mutable std::string m_cached_header;
};

} // namespace nx::net::auth
