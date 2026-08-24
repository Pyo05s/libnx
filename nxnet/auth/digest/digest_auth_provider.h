// 파일: digest_auth_provider.h
// 생성일: 2026-02-10
// 설명: HTTP/RTSP Digest 인증 제공자

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include <nxcore/util/type_util.h>

#include <string>
#include <memory>
#include <atomic>
#include <mutex>

namespace nx::net::auth {

// Digest 인증 제공자 (RFC 7616)
class DigestAuthProvider : public AuthProvider
{
public:
  explicit DigestAuthProvider(Credentials credentials);

  AuthScheme scheme() const noexcept override;

  nx::expected<std::string>
  generate_authorization_header(const AuthContext& context) const override;

  std::error_code process_challenge(const AuthChallenge& challenge) override;

  std::unique_ptr<AuthProvider> clone() const override;

private:
  std::string compute_ha1(
    const std::string& realm, const std::string& nonce, const std::string& cnonce) const;
  std::string compute_ha2(
    const std::string& method,
    const std::string& uri,
    const std::optional<std::string>& body) const;
  std::string compute_response(
    const std::string& ha1,
    const std::string& ha2,
    const std::string& nonce,
    const std::string& nc,
    const std::string& cnonce,
    const std::string& qop) const;
  std::string format_nc(uint32_t nc) const;

  // 알고리즘에 따라 해시 계산
  std::string compute_hash(std::string_view input) const;

  // 알고리즘이 -sess 접미사를 가지는지 확인
  bool is_session_algorithm() const;

  Credentials m_credentials;
  AuthChallenge m_challenge;
  mutable std::atomic<uint32_t> m_nonce_count{0};
  mutable std::mutex m_mutex; // Challenge 업데이트 보호
};

} // namespace nx::net::auth