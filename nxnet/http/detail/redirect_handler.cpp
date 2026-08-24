// 파일: redirect_handler.cpp
// 생성일: 2026-02-06
// 설명: HTTP/HTTPS 리다이렉션 처리 유틸리티 구현

#include "redirect_handler.h"
#include <spdlog/spdlog.h>

namespace {

// RFC 3986 §5.2.4: 경로 내 dot segment(., ..) 제거 정규화
std::string
remove_dot_segments(std::string_view path)
{
  std::string output;
  output.reserve(path.size());

  while (!path.empty()) {
    // A: 입력이 "../" 또는 "./"로 시작하면 제거
    if (path.starts_with("../")) {
      path.remove_prefix(3);
    }
    else if (path.starts_with("./")) {
      path.remove_prefix(2);
    }
    // B: "/./", 또는 끝이 "/."이면 "/"로 대체
    else if (path.starts_with("/./")) {
      path.remove_prefix(2); // → "/.." 이후 다음 루프
    }
    else if (path == "/.") {
      path = "/";
    }
    // C: "/../", 또는 끝이 "/.."이면 "/"로 대체하고 output 마지막 세그먼트 제거
    else if (path.starts_with("/../")) {
      path.remove_prefix(3);
      auto pos = output.rfind('/');
      if (pos != std::string::npos) {
        output.erase(pos);
      }
    }
    else if (path == "/..") {
      path = "/";
      auto pos = output.rfind('/');
      if (pos != std::string::npos) {
        output.erase(pos);
      }
    }
    // D: 단독 "." 또는 ".."이면 제거
    else if (path == "." || path == "..") {
      path = {};
    }
    // E: 첫 번째 path segment를 output으로 이동
    else {
      size_t seg_end = path.find('/', 1);
      size_t end = (seg_end == std::string_view::npos) ? path.size() : seg_end;
      output.append(path.data(), end);
      path.remove_prefix(end);
    }
  }

  return output;
}

} // anonymous namespace

namespace nx {
namespace net {
namespace detail {

bool
RedirectHandler::is_redirect_status(unsigned int status_code) noexcept
{
  return (
    status_code == 301 || status_code == 302 || status_code == 303 ||
    status_code == 307 || status_code == 308);
}

bool
RedirectHandler::should_convert_to_get(
  unsigned int status_code, boost::beast::http::verb method) noexcept
{
  // 303(See Other)는 항상 GET으로 변환
  if (status_code == 303) {
    return true;
  }

  // 301, 302에서 POST는 GET으로 변환
  if ((status_code == 301 || status_code == 302) &&
      method == boost::beast::http::verb::post) {
    return true;
  }

  return false;
}

nx::expected<RedirectInfo>
RedirectHandler::parse_location(
  const std::string& location,
  const std::string& base_host,
  uint16_t base_port,
  const std::string& base_path,
  bool current_is_https)
{
  // HTTPS 절대 URL
  if (location.find("https://") == 0) {
    size_t host_start = 8; // "https://" 길이
    size_t path_start = location.find('/', host_start);

    if (path_start == std::string::npos) {
      // 경로가 없는 경우 (예: https://example.com)
      std::string host_port = location.substr(host_start);
      size_t colon_pos = host_port.find(':');

      if (colon_pos != std::string::npos) {
        std::string host = host_port.substr(0, colon_pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
        return RedirectInfo{host, port, "/", true};
      }
      else {
        return RedirectInfo{host_port, 443, "/", true};
      }
    }
    else {
      std::string host_port = location.substr(host_start, path_start - host_start);
      std::string path = location.substr(path_start);
      size_t colon_pos = host_port.find(':');

      if (colon_pos != std::string::npos) {
        std::string host = host_port.substr(0, colon_pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
        return RedirectInfo{host, port, path, true};
      }
      else {
        return RedirectInfo{host_port, 443, path, true};
      }
    }
  }
  // HTTP 절대 URL
  else if (location.find("http://") == 0) {
    size_t host_start = 7; // "http://" 길이
    size_t path_start = location.find('/', host_start);

    if (path_start == std::string::npos) {
      // 경로가 없는 경우 (예: http://example.com)
      std::string host_port = location.substr(host_start);
      size_t colon_pos = host_port.find(':');

      if (colon_pos != std::string::npos) {
        std::string host = host_port.substr(0, colon_pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
        return RedirectInfo{host, port, "/", false};
      }
      else {
        return RedirectInfo{host_port, 80, "/", false};
      }
    }
    else {
      std::string host_port = location.substr(host_start, path_start - host_start);
      std::string path = location.substr(path_start);
      size_t colon_pos = host_port.find(':');

      if (colon_pos != std::string::npos) {
        std::string host = host_port.substr(0, colon_pos);
        uint16_t port = static_cast<uint16_t>(std::stoi(host_port.substr(colon_pos + 1)));
        return RedirectInfo{host, port, path, false};
      }
      else {
        return RedirectInfo{host_port, 80, path, false};
      }
    }
  }
  // 절대 경로 (같은 호스트, 프로토콜 유지) — dot segment 정규화 적용
  else if (location[0] == '/') {
    return RedirectInfo{
      base_host, base_port, remove_dot_segments(location), current_is_https
    };
  }
  else {
    // RFC 3986 §5.2: 상대 경로는 base_path의 마지막 세그먼트를 교체 후 정규화
    // 예) base_path="/redirect/1", location="../anything" → "/anything"
    // 예) base_path="/redirect/1", location="2"           → "/redirect/2"
    size_t last_slash = base_path.rfind('/');
    std::string dir =
      (last_slash != std::string::npos) ? base_path.substr(0, last_slash + 1) : "/";
    return RedirectInfo{
      base_host, base_port, remove_dot_segments(dir + location), current_is_https
    };
  }
}

std::string
RedirectHandler::find_location_header(const boost::beast::http::fields& headers)
{
  // Beast의 fields는 대소문자를 구분하지 않으므로 한 번만 찾으면 됩니다.
  auto it = headers.find(boost::beast::http::field::location);
  if (it != headers.end()) {
    // beast::http::fields의 반복자는 .value() 메서드를 통해 값을 string_view 형태로
    // 반환합니다. 이를 std::string으로 변환하여 리턴합니다.
    return std::string(it->value());
  }
  return {};
}

void
RedirectHandler::strip_sensitive_headers(boost::beast::http::fields& headers)
{
  // Authorization, Cookie 등 민감한 헤더 제거
  headers.erase("Authorization");
  headers.erase("authorization");
  headers.erase("Cookie");
  headers.erase("cookie");
  headers.erase("Proxy-Authorization");
  headers.erase("proxy-authorization");

  spdlog::debug("Stripped sensitive headers for downgrade");
}

} // namespace detail
} // namespace net
} // namespace nx
