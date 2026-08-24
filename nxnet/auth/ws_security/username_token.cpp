// 파일: username_token.cpp
// 생성일: 2026-02-10
// 설명: WS-Security UsernameToken 생성기 구현

#include "nxnet/auth/ws_security/username_token.h"
#include "nxnet/auth/auth_error.h"
#include "nxcore/crypto/base64.h"
#include "nxcore/crypto/sha1.h"
#include "nxcore/crypto/sha256.h"
#include "nxcore/crypto/sha512.h"
#include "nxcore/crypto/random.h"
#include "nxcore/util/time_util.h"
#include <sstream>

namespace nx::net::auth::ws_security {

nx::expected<UsernameToken>
UsernameTokenBuilder::create_with_digest(
  const Credentials& credentials, HashAlgorithm algorithm)
{
  // 자격 증명 유효성 검증
  if (credentials.username.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidCredentials));
  }

  UsernameToken token;
  token.username = credentials.username;
  token.password = credentials.password;
  token.is_digest = true;
  token.algorithm = algorithm;

  // Nonce 생성 (16바이트)
  token.nonce_bytes = generate_nonce();
  token.nonce_base64 = nx::crypto::Base64::encode(token.nonce_bytes);

  // Timestamp 생성 (ISO 8601 UTC)
  token.created = generate_timestamp();

  // PasswordDigest 계산
  auto digest_result = compute_password_digest(
    token.nonce_bytes,
    token.created,
    token.password,
    algorithm);

  if (!digest_result.has_value()) {
    return std::unexpected(digest_result.error());
  }

  token.password_digest = *digest_result;

  return token;
}

nx::expected<UsernameToken>
UsernameTokenBuilder::create_with_plain_text(const Credentials& credentials)
{
  // 자격 증명 유효성 검증
  if (credentials.username.empty()) {
    return std::unexpected(make_error_code(AuthError::kInvalidCredentials));
  }

  UsernameToken token;
  token.username = credentials.username;
  token.password = credentials.password;
  token.is_digest = false;
  token.algorithm = HashAlgorithm::kSha1; // PlainText 모드에서는 미사용

  // Plain text 모드에서도 nonce와 created는 생성
  token.nonce_bytes = generate_nonce();
  token.nonce_base64 = nx::crypto::Base64::encode(token.nonce_bytes);
  token.created = generate_timestamp();

  return token;
}

std::string
UsernameTokenBuilder::generate_soap_header_xml(const UsernameToken& token)
{
  std::ostringstream oss;

  // OASIS WS-Security 네임스페이스
  oss << "<wsse:Security "
         "xmlns:wsse=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-wssecurity-secext-1.0.xsd\" "
      << "xmlns:wsu=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-wssecurity-utility-1.0.xsd\">\n";

  oss << "  <wsse:UsernameToken>\n";
  oss << "    <wsse:Username>" << token.username << "</wsse:Username>\n";

  if (token.is_digest) {
    // PasswordDigest 모드
    oss << "    <wsse:Password "
           "Type=\"http://docs.oasis-open.org/wss/2004/01/"
           "oasis-200401-wss-username-token-profile-1.0#PasswordDigest\">"
        << token.password_digest << "</wsse:Password>\n";
  }
  else {
    // PasswordText 모드
    oss << "    <wsse:Password "
           "Type=\"http://docs.oasis-open.org/wss/2004/01/"
           "oasis-200401-wss-username-token-profile-1.0#PasswordText\">"
        << token.password << "</wsse:Password>\n";
  }

  oss << "    <wsse:Nonce "
         "EncodingType=\"http://docs.oasis-open.org/wss/2004/01/"
         "oasis-200401-wss-soap-message-security-1.0#Base64Binary\">"
      << token.nonce_base64 << "</wsse:Nonce>\n";
  oss << "    <wsu:Created>" << token.created << "</wsu:Created>\n";
  oss << "  </wsse:UsernameToken>\n";
  oss << "</wsse:Security>";

  return oss.str();
}

nx::crypto::Bytes
UsernameTokenBuilder::generate_nonce()
{
  // 16바이트 암호학적 난수 생성
  return nx::crypto::Random::generate_bytes(16);
}

std::string
UsernameTokenBuilder::generate_timestamp()
{
  // ISO 8601 UTC 타임스탬프: "2026-02-10T08:30:00Z"
  auto iso_result = nx::Timestamp::now().to_iso_string();
  if (iso_result.has_value()) {
    return *iso_result;
  }

  // 실패 시 비어있는 문자열 반환 (호출자가 처리)
  return "";
}

nx::expected<std::string>
UsernameTokenBuilder::compute_password_digest(
  const nx::crypto::Bytes& nonce,
  const std::string& created,
  const std::string& password,
  HashAlgorithm algorithm)
{
  // PasswordDigest = Base64(HASH(nonce + created + password))

  // 1. nonce + created + password 연결
  nx::crypto::Bytes combined;
  combined.reserve(nonce.size() + created.size() + password.size());

  // nonce 추가
  combined.insert(combined.end(), nonce.begin(), nonce.end());

  // created 추가 (UTF-8 바이트로)
  combined.insert(combined.end(), created.begin(), created.end());

  // password 추가 (UTF-8 바이트로)
  combined.insert(combined.end(), password.begin(), password.end());

  // 2. 해시 계산 (알고리즘에 따라)
  nx::crypto::Bytes hash_result;

  switch (algorithm) {
    case HashAlgorithm::kSha1: hash_result = nx::crypto::Sha1::hash(combined); break;
    case HashAlgorithm::kSha256: hash_result = nx::crypto::Sha256::hash(combined); break;
    case HashAlgorithm::kSha512: hash_result = nx::crypto::Sha512::hash(combined); break;
    default: return std::unexpected(make_error_code(AuthError::kHashError));
  }

  if (hash_result.empty()) {
    return std::unexpected(make_error_code(AuthError::kHashError));
  }

  // 3. Base64 인코딩
  std::string digest = nx::crypto::Base64::encode(hash_result);

  return digest;
}

} // namespace nx::net::auth::ws_security
