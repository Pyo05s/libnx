// 파일: auth_challenge_parser.h
// 생성일: 2026-02-10
// 설명: WWW-Authenticate 헤더 파서

#pragma once

#include "nxnet/auth/auth_types.h"
#include "nxnet/auth/auth_error.h"

#include <nxcore/util/type_util.h>

#include <expected>
#include <string_view>
#include <system_error>

namespace nx::net::auth {

// WWW-Authenticate 헤더 파서
class AuthChallengeParser
{
public:
  // 인스턴스화 방지
  AuthChallengeParser() = delete;
  ~AuthChallengeParser() = delete;
  AuthChallengeParser(const AuthChallengeParser&) = delete;
  AuthChallengeParser& operator=(const AuthChallengeParser&) = delete;

  // WWW-Authenticate 헤더 파싱
  // 예: \"Basic realm=\\\"example\\\"\"
  //     \"Digest realm=\\\"example\\\", nonce=\\\"abc123\\\", qop=\\\"auth\\\"\"
  static nx::expected<AuthChallenge> parse(std::string_view www_authenticate_header);

private:
  // 인증 스킴 파싱
  static nx::expected<AuthScheme> parse_scheme(std::string_view scheme_str);

  // 파라미터 파싱 (key=value 또는 key=\\\"value\\\")
  static nx::expected<std::map<std::string, std::string>>
  parse_parameters(std::string_view params_str);

  // 따옴표 제거
  static std::string trim_quotes(std::string_view str);

  // 공백 제거
  static std::string_view trim_whitespace(std::string_view str);
};

} // namespace nx::net::auth