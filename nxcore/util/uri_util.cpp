// 파일: uri_util.cpp
// 생성일: 2026-02-17
// 설명: URI 파싱 및 조작 유틸리티 구현

#include "uri_util.h"
#include <regex>
#include <sstream>
#include <iomanip>
#include <cctype>

namespace nx {

namespace {

// URI 정규식
// scheme://host:port/path?query#fragment
// scheme://user_id:user_password@host:port/path?query#fragment
const std::regex kUriRegex(
  R"(^([a-zA-Z][a-zA-Z0-9+.-]*)://(?:([^:@]+)(?::([^@]*))?@)?([^:\/\?#]+)(?::(\d+))?([^\?#]*)(?:\?([^#]*))?(?:#(.*))?$)");

} // anonymous namespace

// ============================================================================
// URI 파싱 및 생성
// ============================================================================

std::optional<UriComponents>
parse_uri(const std::string& uri)
{
  std::smatch match;
  if (!std::regex_match(uri, match, kUriRegex)) {
    return std::nullopt;
  }

  UriComponents components;
  components.scheme = match[1].str();

  // userinfo 파싱 (user:password@)
  if (match[2].matched) {
    components.username = match[2].str();
  }
  if (match[3].matched) {
    components.password = match[3].str();
  }

  components.host = match[4].str();

  // 포트 파싱
  if (match[5].matched) {
    try {
      int port_value = std::stoi(match[5].str());
      if (port_value > 0 && port_value <= 65535) {
        components.port = static_cast<uint16_t>(port_value);
      }
    }
    catch (...) {
      return std::nullopt;
    }
  }
  else {
    // 기본 포트 설정
    if (components.scheme == "http") {
      components.port = 80;
    }
    else if (components.scheme == "https") {
      components.port = 443;
    }
    else if (components.scheme == "rtsp") {
      components.port = 554;
    }
  }

  components.path = match[6].str();
  components.query = match[7].str();
  components.fragment = match[8].str();

  return components;
}

std::string
build_uri(const UriComponents& components)
{
  std::ostringstream oss;

  // scheme://[user:password@]host:port
  oss << components.scheme << "://";

  // userinfo 추가
  if (!components.username.empty()) {
    oss << components.username;
    if (!components.password.empty()) {
      oss << ":" << components.password;
    }
    oss << "@";
  }

  oss << components.host;

  // 기본 포트가 아닌 경우만 포트 추가
  bool default_port = false;
  if (
    (components.scheme == "http" && components.port == 80)
    || (components.scheme == "https" && components.port == 443)
    || (components.scheme == "rtsp" && components.port == 554)) {
    default_port = true;
  }

  if (!default_port && components.port > 0) {
    oss << ":" << components.port;
  }

  // /path
  oss << components.path;

  // ?query
  if (!components.query.empty()) {
    oss << "?" << components.query;
  }

  // #fragment
  if (!components.fragment.empty()) {
    oss << "#" << components.fragment;
  }

  return oss.str();
}

std::string
replace_uri_authority(
  const std::string& uri, const std::string& new_host, uint16_t new_port)
{
  auto components = parse_uri(uri);
  if (!components) {
    return uri; // 파싱 실패 시 원본 반환
  }

  components->host = new_host;
  components->port = new_port;

  return build_uri(*components);
}

std::string
replace_uri_host(const std::string& uri, const std::string& new_host)
{
  auto components = parse_uri(uri);
  if (!components) {
    return uri;
  }

  components->host = new_host;

  return build_uri(*components);
}

std::string
replace_uri_port(const std::string& uri, uint16_t new_port)
{
  auto components = parse_uri(uri);
  if (!components) {
    return uri;
  }

  components->port = new_port;

  return build_uri(*components);
}

std::string
url_encode(const std::string& str)
{
  std::ostringstream oss;
  oss << std::hex << std::uppercase;

  for (unsigned char c : str) {
    // RFC 3986 unreserved characters
    if (std::isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
      oss << c;
    }
    else {
      oss << '%' << std::setw(2) << std::setfill('0') << static_cast<int>(c);
    }
  }

  return oss.str();
}

std::string
url_decode(const std::string& str)
{
  std::ostringstream oss;

  for (size_t i = 0; i < str.size(); ++i) {
    if (str[i] == '%' && i + 2 < str.size()) {
      // %XX 형식 디코딩
      std::string hex = str.substr(i + 1, 2);
      try {
        int value = std::stoi(hex, nullptr, 16);
        oss << static_cast<char>(value);
        i += 2;
      }
      catch (...) {
        oss << str[i]; // 디코딩 실패 시 원본 유지
      }
    }
    else if (str[i] == '+') {
      oss << ' '; // '+' -> 공백
    }
    else {
      oss << str[i];
    }
  }

  return oss.str();
}

} // namespace nx
