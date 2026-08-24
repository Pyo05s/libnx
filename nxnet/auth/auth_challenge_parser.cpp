// 파일: auth_challenge_parser.cpp
// 생성일: 2026-02-10
// 설명: WWW-Authenticate 헤더 파서 구현

#include "nxnet/auth/auth_challenge_parser.h"
#include <algorithm>
#include <sstream>
#include <cctype>

namespace nx::net::auth {

namespace {

// 문자열을 소문자로 변환
std::string
to_lower(std::string_view str)
{
  std::string result;
  result.reserve(str.size());
  std::transform(str.begin(), str.end(), std::back_inserter(result), [](unsigned char c) {
    return std::tolower(c);
  });
  return result;
}

} // anonymous namespace

nx::expected<AuthChallenge>
AuthChallengeParser::parse(std::string_view www_authenticate_header)
{
  if (www_authenticate_header.empty()) {
    return std::unexpected(make_error_code(AuthError::kNoChallenge));
  }

  // 스킴과 파라미터 분리
  auto space_pos = www_authenticate_header.find(' ');
  if (space_pos == std::string_view::npos) {
    return std::unexpected(make_error_code(AuthError::kInvalidChallenge));
  }

  auto scheme_str = www_authenticate_header.substr(0, space_pos);
  auto params_str = www_authenticate_header.substr(space_pos + 1);

  // 스킴 파싱
  auto scheme_result = parse_scheme(scheme_str);
  if (!scheme_result) {
    return std::unexpected(scheme_result.error());
  }

  // 파라미터 파싱
  auto params_result = parse_parameters(params_str);
  if (!params_result) {
    return std::unexpected(params_result.error());
  }

  // AuthChallenge 구성
  AuthChallenge challenge;
  challenge.scheme = *scheme_result;

  const auto& params = *params_result;

  // realm (필수)
  auto realm_it = params.find("realm");
  if (realm_it != params.end()) {
    challenge.realm = realm_it->second;
  }

  // nonce (Digest에서 필수)
  auto nonce_it = params.find("nonce");
  if (nonce_it != params.end()) {
    challenge.nonce = nonce_it->second;
  }

  // opaque (선택)
  auto opaque_it = params.find("opaque");
  if (opaque_it != params.end()) {
    challenge.opaque = opaque_it->second;
  }

  // algorithm (선택)
  auto algorithm_it = params.find("algorithm");
  if (algorithm_it != params.end()) {
    challenge.algorithm = algorithm_it->second;
  }

  // qop (선택)
  auto qop_it = params.find("qop");
  if (qop_it != params.end()) {
    challenge.qop = qop_it->second;
  }

  // stale (선택)
  auto stale_it = params.find("stale");
  if (stale_it != params.end()) {
    std::string stale_lower = to_lower(stale_it->second);
    challenge.stale = (stale_lower == "true");
  }

  // extra_params (기타 모든 파라미터)
  for (const auto& [key, value] : params) {
    if (
      key != "realm" && key != "nonce" && key != "opaque" && key != "algorithm"
      && key != "qop" && key != "stale") {
      challenge.extra_params[key] = value;
    }
  }

  return challenge;
}

nx::expected<AuthScheme>
AuthChallengeParser::parse_scheme(std::string_view scheme_str)
{
  std::string scheme_lower = to_lower(scheme_str);

  if (scheme_lower == "basic") {
    return AuthScheme::kBasic;
  }
  else if (scheme_lower == "digest") {
    return AuthScheme::kDigest;
  }
  else if (scheme_lower == "bearer") {
    return AuthScheme::kBearer;
  }
  else {
    return std::unexpected(make_error_code(AuthError::kUnsupportedScheme));
  }
}

nx::expected<std::map<std::string, std::string>>
AuthChallengeParser::parse_parameters(std::string_view params_str)
{
  std::map<std::string, std::string> params;

  size_t pos = 0;
  while (pos < params_str.length()) {
    // 공백 건너뛰기
    while (pos < params_str.length() && std::isspace(params_str[pos])) {
      ++pos;
    }

    if (pos >= params_str.length()) {
      break;
    }

    // key 찾기
    size_t equal_pos = params_str.find('=', pos);
    if (equal_pos == std::string_view::npos) {
      break;
    }

    std::string key(trim_whitespace(params_str.substr(pos, equal_pos - pos)));
    pos = equal_pos + 1;

    // value 찾기
    std::string value;
    if (pos < params_str.length()) {
      if (params_str[pos] == '"') {
        // 따옴표로 감싸진 값
        ++pos; // 시작 따옴표 건너뛰기
        size_t end_quote = params_str.find('"', pos);
        if (end_quote != std::string_view::npos) {
          value = std::string(params_str.substr(pos, end_quote - pos));
          pos = end_quote + 1;
        }
        else {
          return std::unexpected(make_error_code(AuthError::kInvalidParameter));
        }
      }
      else {
        // 따옴표 없는 값 (쉼표 또는 끝까지)
        size_t comma_pos = params_str.find(',', pos);
        if (comma_pos != std::string_view::npos) {
          value = std::string(trim_whitespace(params_str.substr(pos, comma_pos - pos)));
          pos = comma_pos + 1;
        }
        else {
          value = std::string(trim_whitespace(params_str.substr(pos)));
          pos = params_str.length();
        }
      }
    }

    if (!key.empty()) {
      params[key] = value;
    }

    // 쉼표 건너뛰기
    while (pos < params_str.length()
           && (params_str[pos] == ',' || std::isspace(params_str[pos]))) {
      ++pos;
    }
  }

  return params;
}

std::string
AuthChallengeParser::trim_quotes(std::string_view str)
{
  if (str.size() >= 2 && str.front() == '"' && str.back() == '"') {
    return std::string(str.substr(1, str.size() - 2));
  }
  return std::string(str);
}

std::string_view
AuthChallengeParser::trim_whitespace(std::string_view str)
{
  auto start = str.find_first_not_of(" \t\r\n");
  if (start == std::string_view::npos) {
    return {};
  }

  auto end = str.find_last_not_of(" \t\r\n");
  return str.substr(start, end - start + 1);
}

} // namespace nx::net::auth