// 파일: username_token.h
// 생성일: 2026-02-10
// 설명: WS-Security UsernameToken 생성기

#pragma once

#include "nxnet/auth/ws_security/ws_security_types.h"
#include "nxnet/auth/auth_types.h"
#include <nxcore/util/type_util.h>

#include <expected>
#include <system_error>

namespace nx::net::auth::ws_security {

// UsernameToken 생성기
class UsernameTokenBuilder
{
public:
  // PasswordDigest 형식의 UsernameToken 생성
  // algorithm: 해시 알고리즘 (기본값: SHA-1)
  static nx::expected<UsernameToken> create_with_digest(
    const Credentials& credentials, HashAlgorithm algorithm = HashAlgorithm::kSha1);

  // PasswordText 형식의 UsernameToken 생성 (보안상 권장하지 않음)
  static nx::expected<UsernameToken>
  create_with_plain_text(const Credentials& credentials);

  // SOAP Security 헤더 XML 생성
  static std::string generate_soap_header_xml(const UsernameToken& token);

private:
  // 16바이트 Nonce 생성
  static nx::crypto::Bytes generate_nonce();

  // ISO 8601 UTC 타임스탬프 생성
  static std::string generate_timestamp();

  // PasswordDigest 계산: Base64(HASH(nonce + created + password))
  static nx::expected<std::string> compute_password_digest(
    const nx::crypto::Bytes& nonce,
    const std::string& created,
    const std::string& password,
    HashAlgorithm algorithm);
};

} // namespace nx::net::auth::ws_security
