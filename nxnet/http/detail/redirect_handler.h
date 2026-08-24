// 파일: redirect_handler.h
// 생성일: 2026-02-06
// 설명: HTTP/HTTPS 리다이렉션 처리를 위한 유틸리티 함수

#pragma once

#include "../http_error.h"

#include <nxcore/util/type_util.h>

#include <boost/beast/http.hpp>

#include <expected>
#include <map>
#include <string>
#include <system_error>
#include <tuple>

namespace nx {
namespace net {
namespace detail {

// 리다이렉션 정보 구조체
struct RedirectInfo
{
  std::string host;   // 리다이렉션 대상 호스트
  uint16_t port;      // 리다이렉션 대상 포트
  std::string target; // 리다이렉션 대상 경로
  bool is_https;      // HTTPS 여부 (true: https://, false: http://)
};

// HTTP 리다이렉션 처리 유틸리티 클래스
class RedirectHandler
{
public:
  // 리다이렉션 상태 코드 확인 (301, 302, 303, 307, 308)
  static bool is_redirect_status(unsigned int status_code) noexcept;

  // POST→GET 메서드 변환 필요 여부
  // 301, 302, 303 상태 코드에서 POST 요청은 GET으로 변환
  static bool should_convert_to_get(
    unsigned int status_code, boost::beast::http::verb method) noexcept;

  // Location 헤더 URL 파싱
  // location: Location 헤더 값 (절대 URL 또는 상대 경로)
  // base_host: 현재 연결된 호스트
  // base_port: 현재 연결된 포트
  // base_path: 현재 요청 경로 (상대 경로 해석 기준, RFC 3986)
  // current_is_https: 현재 연결이 HTTPS인지 여부
  static nx::expected<RedirectInfo> parse_location(
    const std::string& location,
    const std::string& base_host,
    uint16_t base_port,
    const std::string& base_path,
    bool current_is_https);

  // Location 헤더 찾기 (대소문자 무관)
  static std::string find_location_header(const boost::beast::http::fields& headers);

  // 민감한 헤더 제거 (다운그레이드 시 보안)
  // Authorization, Cookie, Proxy-Authorization 등 제거
  static void strip_sensitive_headers(boost::beast::http::fields& headers);
};

} // namespace detail
} // namespace net
} // namespace nx
