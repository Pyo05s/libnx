// 파일: auth_types.h
// 생성일: 2026-02-10
// 설명: 네트워크 인증 모듈 공통 타입 정의

#pragma once

#include <string>
#include <optional>
#include <map>

namespace nx::net::auth {

// 인증 스킴
enum class AuthScheme
{
  kNone,
  kBasic,
  kDigest,
  kBearer,
  kApiKey,
  kWsSecurity
};

// 자격 증명
struct Credentials
{
  std::string username;
  std::string password;
};

// 인증 Challenge (WWW-Authenticate 헤더 파싱 결과)
struct AuthChallenge
{
  AuthScheme scheme = AuthScheme::kNone;
  std::string realm;
  std::optional<std::string> nonce;
  std::optional<std::string> opaque;
  std::optional<std::string> algorithm; // \"MD5\", \"SHA-256\"
  std::optional<std::string> qop;       // \"auth\", \"auth-int\"
  std::optional<bool> stale;
  std::map<std::string, std::string> extra_params;
};

// 인증 컨텍스트 (요청 정보)
struct AuthContext
{
  std::string method;              // \"GET\", \"DESCRIBE\", \"POST\" 등
  std::string uri;                 // 요청 URI
  std::optional<std::string> body; // 요청 본문 (auth-int용)
};

} // namespace nx::net::auth