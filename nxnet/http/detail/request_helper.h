// 파일: request_helper.h
// 생성일: 2026-02-06
// 설명: HTTP/HTTPS 클라이언트 공통 요청/응답 처리 유틸리티

#pragma once

#include "../http_types.h"
#include "../http_error.h"
#include <boost/beast/http.hpp>
#include <atomic>
#include <expected>
#include <system_error>

namespace nx {
namespace net {
namespace detail {

/// @brief HTTP 요청/응답 처리 헬퍼 클래스
class RequestHelper
{
public:
  /// @brief Beast HTTP request 객체 생성
  /// @param request 요청 정보
  /// @param host 호스트 이름
  /// @param user_agent User-Agent 문자열 (예: "nx-http-client/1.0")
  /// @return Beast HTTP request 객체
  static boost::beast::http::request<boost::beast::http::string_body> build_request(
    const HttpRequest& request,
    const std::string& host,
    const std::string& user_agent);

  /// @brief Beast HTTP response를 HttpResponse로 변환
  /// @param beast_response Beast 응답 객체
  /// @return 변환된 HttpResponse
  static HttpResponse convert_response(
    const boost::beast::http::response<boost::beast::http::string_body>&
      beast_response);

  /// @brief 요청 중 발생한 에러를 HTTP 에러 코드로 변환
  /// @param ex Boost system_error 예외
  /// @param connected_flag 연결 상태 플래그 (연결 종료 시 false로 설정)
  /// @param request_target 요청 대상 (로깅용)
  /// @return HTTP 에러 코드
  static std::error_code handle_request_error(
    const boost::system::system_error& ex,
    std::atomic<bool>& connected_flag,
    const std::string& request_target);
};

} // namespace detail
} // namespace net
} // namespace nx
