// 파일: auth_provider.h
// 생성일: 2026-02-10
// 설명: 네트워크 인증 제공자 추상 인터페이스 및 팩토리

#pragma once

#include "nxnet/auth/auth_error.h"
#include "nxnet/auth/auth_types.h"

#include <nxcore/util/type_util.h>

#include <boost/beast/http.hpp>

#include <expected>
#include <memory>
#include <system_error>

namespace nx::net::auth {

// 추상 인증 제공자 인터페이스
class AuthProvider
{
public:
  virtual ~AuthProvider() = default;

  // 인증 스킴 식별
  virtual AuthScheme scheme() const noexcept = 0;

  // HTTP/RTSP Authorization 헤더 생성
  virtual nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const = 0;

  // HTTP 헤더 전체 생성 (헤더 이름 + 값)
  // 기본 구현: Authorization 헤더 사용
  // API Key 등 커스텀 헤더가 필요한 경우 오버라이드
  virtual nx::expected<boost::beast::http::fields>
  generate_headers(const AuthContext& context) const
  {
    auto auth_value = generate_authorization_header(context);
    if (!auth_value.has_value())
      return std::unexpected(auth_value.error());

    boost::beast::http::fields headers;
    headers.set(boost::beast::http::field::authorization, *auth_value);
    return headers;
  }

  // Challenge 응답 처리 (Digest 인증에서 사용)
  virtual std::error_code process_challenge(const AuthChallenge& /*challenge*/)
  {
    return {}; // 기본적으로 무시
  }

  // SOAP Security 헤더 생성 (WS-Security만 구현)
  virtual nx::expected<std::string> generate_soap_security_header() const
  {
    return std::unexpected(make_error_code(std::errc::not_supported));
  }

  // 인증 제공자 복제 (다중 연결 지원)
  virtual std::unique_ptr<AuthProvider> clone() const = 0;
};

// 인증 제공자 팩토리
class AuthProviderFactory
{
public:
  // 인스턴스화 방지
  AuthProviderFactory() = delete;
  ~AuthProviderFactory() = delete;
  AuthProviderFactory(const AuthProviderFactory&) = delete;
  AuthProviderFactory& operator=(const AuthProviderFactory&) = delete;
  AuthProviderFactory(AuthProviderFactory&&) = delete;
  AuthProviderFactory& operator=(AuthProviderFactory&&) = delete;

  // Basic 인증 생성
  static std::unique_ptr<AuthProvider> create_basic(const Credentials& credentials);

  // Digest 인증 생성
  static std::unique_ptr<AuthProvider> create_digest(const Credentials& credentials);

  // Bearer Token 생성
  static std::unique_ptr<AuthProvider> create_bearer(std::string token);

  // API Key 생성
  static std::unique_ptr<AuthProvider>
  create_api_key(std::string key, std::string header_name = "X-API-Key");

  // WS-Security 생성
  static std::unique_ptr<AuthProvider>
  create_ws_security(const Credentials& credentials, bool use_digest = true);
};

} // namespace nx::net::auth