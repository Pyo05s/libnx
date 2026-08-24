// 파일: http_types.h
// 생성일: 2026-02-06
// 설명: HTTP 클라이언트 공통 타입 정의

#pragma once

#include <nxcore/util/time_util.h>

#include <boost/beast/http.hpp>

#include <map>
#include <string>

namespace nx {
namespace net {

/// @brief HTTP 요청 구조체
struct HttpRequest
{
  boost::beast::http::verb method;    // GET, POST, PUT, DELETE 등
  std::string target;                 // 요청 경로 (예: "/api/v1/users")
  std::string body;                   // 요청 본문
  boost::beast::http::fields headers; // 추가 헤더
};

/// @brief HTTP 응답 구조체
struct HttpResponse
{
  unsigned int status_code;           // HTTP 상태 코드 (200, 404 등)
  std::string body;                   // 응답 본문
  boost::beast::http::fields headers; // 응답 헤더
};

/// @brief HTTP 클라이언트 옵션
struct HttpClientOptions
{
  nx::milliseconds connect_timeout{5000};   // 연결 타임아웃 (기본 5초)
  nx::milliseconds response_timeout{30000}; // 응답 타임아웃 (기본 30초)
  bool follow_redirects{true};              // 리다이렉션 자동 처리 (기본 활성화)
  uint32_t max_redirects{5};                // 최대 리다이렉션 횟수 (기본 5회)
};

} // namespace net
} // namespace nx
