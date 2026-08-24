// 파일: request_helper.cpp
// 생성일: 2026-02-06
// 설명: HTTP/HTTPS 클라이언트 공통 요청/응답 처리 유틸리티 구현

#include "request_helper.h"
#include "../http_error.h"
#include <spdlog/spdlog.h>

namespace nx {
namespace net {
namespace detail {

// ========================================================================
// Beast Request 빌드
// ========================================================================
boost::beast::http::request<boost::beast::http::string_body>
RequestHelper::build_request(
  const HttpRequest& request, const std::string& host, const std::string& user_agent)
{
  // HTTP/1.1 요청 객체 생성
  boost::beast::http::request<boost::beast::http::string_body> req{
    request.method, request.target,
    11 // HTTP/1.1
  };

  // 기본 헤더 설정
  req.set(boost::beast::http::field::host, host);
  req.set(boost::beast::http::field::user_agent, user_agent);

  // Body 설정
  req.body() = request.body;
  req.prepare_payload();

  // 사용자 정의 헤더 추가
  for (const auto& field : request.headers) {
    if (field.name() != boost::beast::http::field::unknown) {
      // 1. 표준 헤더(Accept, Host 등)는 enum으로 직접 삽입 (손해 없음, 최고 속도)
      req.insert(field.name(), field.value());
    }
    else {
      // 2. 커스텀 헤더(X-API-Key 등)만 어쩔 수 없이 문자열로 삽입
      req.insert(field.name_string(), field.value());
    }
  }

  return req;
}

// ========================================================================
// Beast Response 변환
// ========================================================================

HttpResponse
RequestHelper::convert_response(
  const boost::beast::http::response<boost::beast::http::string_body>& beast_response)
{
  // HttpResponse 구조체 초기화
  HttpResponse response{
    .status_code = beast_response.result_int(),
    .body = beast_response.body(),
    .headers = {}
  };

  // 모든 헤더 복사
  // 동일한 헤더 이름이 여러 번 나타날 수 있으므로, 중복 헤더를 허용 (insert 사용)
  for (const auto& field : beast_response) {
    if (field.name() != boost::beast::http::field::unknown) {
      // 1. 표준 헤더(Accept, Host 등)는 enum으로 직접 삽입 (손해 없음, 최고 속도)
      response.headers.insert(field.name(), field.value());
    }
    else {
      // 2. 커스텀 헤더(X-API-Key 등)만 어쩔 수 없이 문자열로 삽입
      response.headers.insert(field.name_string(), field.value());
    }
  }

  return response;
}

// ========================================================================
// 요청 에러 처리
// ========================================================================

std::error_code
RequestHelper::handle_request_error(
  const boost::system::system_error& ex,
  std::atomic<bool>& connected_flag,
  const std::string& request_target)
{
  const auto& ec = ex.code();

  // 타임아웃 에러
  if (ec == boost::asio::error::timed_out || ec == boost::beast::error::timeout) {
    spdlog::error("Response timeout for {}", request_target);
    return make_error_code(HttpErrc::response_timeout);
  }

  // 연결 종료 에러 (asio eof, connection_reset, beast end_of_stream)
  if (ec == boost::asio::error::eof || ec == boost::asio::error::connection_reset ||
      ec == boost::beast::http::error::end_of_stream) {
    connected_flag.store(false);
    spdlog::error("Connection closed by peer");
    return make_error_code(HttpErrc::connection_closed);
  }

  // 일반 전송 실패
  spdlog::error("Request failed for {}: {}", request_target, ex.what());
  return make_error_code(HttpErrc::send_failed);
}

} // namespace detail
} // namespace net
} // namespace nx
