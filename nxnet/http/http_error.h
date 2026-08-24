// 파일: http_error.h
// 생성일: 2026-02-06
// 설명: HTTP 클라이언트 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx {
namespace net {

// HTTP 오류 코드
enum class HttpErrc
{
  success = 0,

  // 연결 관련 오류
  connection_failed = 1, // 연결 실패
  connection_closed = 2, // 연결이 닫힘
  connect_timeout = 3,   // 연결 타임아웃
  already_connected = 4, // 이미 연결됨
  not_connected = 5,     // 연결되지 않음

  // 요청/응답 관련 오류
  response_timeout = 10, // 응답 타임아웃
  invalid_response = 11, // 잘못된 응답
  send_failed = 12,      // 전송 실패
  receive_failed = 13,   // 수신 실패

  // HTTP 상태 코드 기반 오류
  bad_request = 400,         // 잘못된 요청
  unauthorized = 401,        // 인증 실패
  forbidden = 403,           // 접근 거부
  not_found = 404,           // 리소스 없음
  server_error = 500,        // 서버 오류
  service_unavailable = 503, // 서비스 사용 불가

  // 기타 오류
  invalid_argument = 20, // 잘못된 인자
  unknown_error = 99,    // 알 수 없는 오류

  // SSL/TLS 관련 오류
  ssl_handshake_failed = 50,    // SSL 핸드셰이크 실패
  ssl_certificate_error = 51,   // 인증서 오류
  ssl_verification_failed = 52, // 인증서 검증 실패

  // 리다이렉션 관련 오류
  too_many_redirects = 60,  // 리다이렉션 횟수 초과
  invalid_redirect = 61,    // 잘못된 리다이렉션
  https_required = 62,      // HTTPS 전환 필요
  downgrade_forbidden = 63, // HTTPS → HTTP 다운그레이드 금지
};

// HTTP 오류 카테고리
class HttpErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::net::http"; }

  std::string message(int ev) const override
  {
    switch (static_cast<HttpErrc>(ev)) {
      case HttpErrc::success:
        return "Success";

        // 연결 관련
      case HttpErrc::connection_failed: return "Connection failed";
      case HttpErrc::connection_closed: return "Connection closed";
      case HttpErrc::connect_timeout: return "Connection timeout";
      case HttpErrc::already_connected: return "Already connected";
      case HttpErrc::not_connected:
        return "Not connected";

        // 요청/응답 관련
      case HttpErrc::response_timeout: return "Response timeout";
      case HttpErrc::invalid_response: return "Invalid response";
      case HttpErrc::send_failed: return "Send failed";
      case HttpErrc::receive_failed:
        return "Receive failed";

        // HTTP 상태 코드 기반
      case HttpErrc::bad_request: return "Bad request (400)";
      case HttpErrc::unauthorized: return "Unauthorized (401)";
      case HttpErrc::forbidden: return "Forbidden (403)";
      case HttpErrc::not_found: return "Not found (404)";
      case HttpErrc::server_error: return "Internal server error (500)";
      case HttpErrc::service_unavailable:
        return "Service unavailable (503)";

        // 기타
      case HttpErrc::invalid_argument: return "Invalid argument";
      case HttpErrc::unknown_error:
        return "Unknown error";

        // SSL/TLS 관련
      case HttpErrc::ssl_handshake_failed: return "SSL handshake failed";
      case HttpErrc::ssl_certificate_error: return "SSL certificate error";
      case HttpErrc::ssl_verification_failed:
        return "SSL certificate verification failed";

        // 리다이렉션 관련
      case HttpErrc::too_many_redirects: return "Too many redirects";
      case HttpErrc::invalid_redirect: return "Invalid redirect response";
      case HttpErrc::https_required: return "HTTPS connection required";
      case HttpErrc::downgrade_forbidden: return "HTTPS to HTTP downgrade forbidden";

      default: return "Unknown HTTP error";
    }
  }
};

// 싱글톤 카테고리 인스턴스
inline const HttpErrorCategory&
http_category()
{
  static HttpErrorCategory instance;
  return instance;
}

// error_code 생성 헬퍼
inline std::error_code
make_error_code(HttpErrc e)
{
  return {static_cast<int>(e), http_category()};
}

} // namespace net
} // namespace nx

// std::error_code와 통합
namespace std {
template <>
struct is_error_code_enum<nx::net::HttpErrc> : true_type
{};
} // namespace std
