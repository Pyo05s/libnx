// 파일: digest_auth_provider.cpp
// 생성일: 2026-02-10
// 설명: HTTP/RTSP Digest 인증 제공자 구현

#include "nxnet/auth/digest/digest_auth_provider.h"
#include "nxnet/auth/auth_error.h"
#include "nxcore/crypto/md5.h"
#include "nxcore/crypto/sha256.h"
#include "nxcore/crypto/sha512.h"
#include "nxcore/crypto/random.h"
#include <sstream>
#include <iomanip>
#include <algorithm>

namespace nx::net::auth {

DigestAuthProvider::DigestAuthProvider(Credentials credentials)
    : m_credentials(std::move(credentials))
{}

AuthScheme
DigestAuthProvider::scheme() const noexcept
{
  return AuthScheme::kDigest;
}

nx::expected<std::string>
DigestAuthProvider::generate_authorization_header(const AuthContext& context) const
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // Challenge가 없으면 생성 불가
  if (m_challenge.scheme == AuthScheme::kNone) {
    return std::unexpected(make_error_code(AuthError::kNoChallenge));
  }

  // Nonce가 필수
  if (!m_challenge.nonce.has_value()) {
    return std::unexpected(make_error_code(AuthError::kMissingParameter));
  }

  // Nonce Count 증가
  uint32_t nc = ++m_nonce_count;
  std::string nc_str = format_nc(nc);

  // Client Nonce 생성
  std::string cnonce = nx::crypto::Random::generate_alphanumeric(16);

  // HA1 계산 (sess 알고리즘의 경우 nonce와 cnonce 필요)
  std::string ha1 = compute_ha1(m_challenge.realm, *m_challenge.nonce, cnonce);

  // HA2 계산
  std::string ha2 = compute_ha2(context.method, context.uri, context.body);

  // Response 계산
  std::string qop_value = m_challenge.qop.value_or("");
  std::string response
    = compute_response(ha1, ha2, *m_challenge.nonce, nc_str, cnonce, qop_value);

  // Authorization 헤더 조립
  std::ostringstream auth_header;
  auth_header << "Digest username=\"" << m_credentials.username << "\""
              << ", realm=\"" << m_challenge.realm << "\""
              << ", nonce=\"" << *m_challenge.nonce << "\""
              << ", uri=\"" << context.uri << "\""
              << ", response=\"" << response << "\"";

  // QoP가 있으면 추가
  if (m_challenge.qop.has_value()) {
    auth_header << ", qop=" << qop_value << ", nc=" << nc_str << ", cnonce=\"" << cnonce
                << "\"";
  }

  // Algorithm 추가 (기본값: MD5)
  if (m_challenge.algorithm.has_value()) {
    auth_header << ", algorithm=" << *m_challenge.algorithm;
  }

  // Opaque 추가
  if (m_challenge.opaque.has_value()) {
    auth_header << ", opaque=\"" << *m_challenge.opaque << "\"";
  }

  return auth_header.str();
}

std::error_code
DigestAuthProvider::process_challenge(const AuthChallenge& challenge)
{
  std::lock_guard<std::mutex> lock(m_mutex);

  // Digest 스킴 확인
  if (challenge.scheme != AuthScheme::kDigest) {
    return make_error_code(AuthError::kInvalidChallenge);
  }

  // Nonce 필수
  if (!challenge.nonce.has_value()) {
    return make_error_code(AuthError::kMissingParameter);
  }

  // Challenge 저장
  m_challenge = challenge;

  // Nonce Count 리셋 (새로운 Challenge)
  m_nonce_count = 0;

  return {};
}

std::unique_ptr<AuthProvider>
DigestAuthProvider::clone() const
{
  auto cloned = std::make_unique<DigestAuthProvider>(m_credentials);

  // Challenge 복사
  std::lock_guard<std::mutex> lock(m_mutex);
  cloned->m_challenge = m_challenge;
  cloned->m_nonce_count = m_nonce_count.load();

  return cloned;
}

std::string
DigestAuthProvider::compute_ha1(
  const std::string& realm, const std::string& nonce, const std::string& cnonce) const
{
  // 기본 HA1 계산: Hash(username:realm:password)
  std::string ha1_base_input
    = m_credentials.username + ":" + realm + ":" + m_credentials.password;
  std::string ha1_base = compute_hash(ha1_base_input);

  // -sess 알고리즘의 경우: Hash(HA1_base:nonce:cnonce)
  if (is_session_algorithm()) {
    std::string ha1_sess_input = ha1_base + ":" + nonce + ":" + cnonce;
    return compute_hash(ha1_sess_input);
  }

  return ha1_base;
}

std::string
DigestAuthProvider::compute_ha2(
  const std::string& method,
  const std::string& uri,
  const std::optional<std::string>& body) const
{
  // QoP가 "auth-int"이면 body hash 포함
  bool is_auth_int = m_challenge.qop.has_value() && (*m_challenge.qop == "auth-int");

  if (is_auth_int && body.has_value()) {
    // HA2 = Hash(method:uri:Hash(body))
    std::string body_hash = compute_hash(*body);
    std::string ha2_input = method + ":" + uri + ":" + body_hash;
    return compute_hash(ha2_input);
  }
  else {
    // HA2 = Hash(method:uri)
    std::string ha2_input = method + ":" + uri;
    return compute_hash(ha2_input);
  }
}

std::string
DigestAuthProvider::compute_response(
  const std::string& ha1,
  const std::string& ha2,
  const std::string& nonce,
  const std::string& nc,
  const std::string& cnonce,
  const std::string& qop) const
{
  std::string response_input;

  if (!qop.empty()) {
    // Response = Hash(HA1:nonce:nc:cnonce:qop:HA2)
    response_input = ha1 + ":" + nonce + ":" + nc + ":" + cnonce + ":" + qop + ":" + ha2;
  }
  else {
    // Response = Hash(HA1:nonce:HA2)
    response_input = ha1 + ":" + nonce + ":" + ha2;
  }

  return compute_hash(response_input);
}

std::string
DigestAuthProvider::format_nc(uint32_t nc) const
{
  // Nonce Count를 8자리 16진수로 포맷 (예: 00000001)
  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(8) << std::hex << nc;
  return oss.str();
}

std::string
DigestAuthProvider::compute_hash(std::string_view input) const
{
  // Challenge의 algorithm 필드에 따라 해시 함수 선택
  // RFC 2617: algorithm 미지정 시 기본값은 MD5
  std::string algorithm = m_challenge.algorithm.value_or("MD5");

  // 대소문자 구분 없이 비교 (RFC 7616)
  std::string algo_lower = algorithm;
  std::transform(
    algo_lower.begin(),
    algo_lower.end(),
    algo_lower.begin(),
    [](unsigned char c) { return std::tolower(c); });

  // -sess 접미사 제거 (해시 함수는 동일)
  if (algo_lower.ends_with("-sess")) {
    algo_lower = algo_lower.substr(0, algo_lower.length() - 5);
  }

  if (algo_lower == "sha-512-256" || algo_lower == "sha512-256") {
    // RFC 7616: SHA-512의 앞쪽 256비트만 사용
    return nx::crypto::Sha512::hash_hex_256(input);
  }
  else if (algo_lower == "sha-512" || algo_lower == "sha512") {
    return nx::crypto::Sha512::hash_hex(input);
  }
  else if (algo_lower == "sha-256" || algo_lower == "sha256") {
    return nx::crypto::Sha256::hash_hex(input);
  }
  else if (algo_lower == "md5") {
    return nx::crypto::Md5::hash_hex(input);
  }
  else {
    // 지원하지 않는 알고리즘은 SHA-256으로 폴백 (보안 강화)
    return nx::crypto::Sha256::hash_hex(input);
  }
}

bool
DigestAuthProvider::is_session_algorithm() const
{
  if (!m_challenge.algorithm.has_value()) {
    return false;
  }

  std::string algo_lower = *m_challenge.algorithm;
  std::transform(
    algo_lower.begin(),
    algo_lower.end(),
    algo_lower.begin(),
    [](unsigned char c) { return std::tolower(c); });

  return algo_lower.ends_with("-sess");
}

} // namespace nx::net::auth