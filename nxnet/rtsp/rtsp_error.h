// 파일: rtsp_error.h
// 생성일: 2026-02-23
// 설명: RTSP 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx::net {

// RTSP 오류 코드
enum class RtspErrc
{
  success = 0,

  // 연결 관련
  connection_failed = 1,
  connection_closed = 2,
  connect_timeout = 3,
  already_connected = 4,
  not_connected = 5,

  // 요청/응답 관련
  response_timeout = 10,
  invalid_response = 11,
  send_failed = 12,
  receive_failed = 13,
  invalid_request = 14,

  // RTSP 프로토콜 관련
  unauthorized = 401,
  forbidden = 403,
  not_found = 404,
  method_not_allowed = 405,
  session_not_found = 454,
  unsupported_transport = 461,
  server_error = 500,
  service_unavailable = 503,

  // 세션 관련
  invalid_state = 20,
  session_expired = 21,
  setup_required = 22,

  // 전송 관련
  transport_error = 30,
  invalid_transport = 31,

  // SDP 관련
  sdp_parse_error = 40,

  // 기타
  invalid_url = 50,
  unknown_error = 99
};

class RtspErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::net::rtsp"; }

  std::string message(int ev) const override
  {
    switch (static_cast<RtspErrc>(ev)) {
      case RtspErrc::success: return "Success";
      case RtspErrc::connection_failed: return "Connection failed";
      case RtspErrc::connection_closed: return "Connection closed";
      case RtspErrc::connect_timeout: return "Connection timeout";
      case RtspErrc::already_connected: return "Already connected";
      case RtspErrc::not_connected: return "Not connected";
      case RtspErrc::response_timeout: return "Response timeout";
      case RtspErrc::invalid_response: return "Invalid RTSP response";
      case RtspErrc::send_failed: return "Send failed";
      case RtspErrc::receive_failed: return "Receive failed";
      case RtspErrc::invalid_request: return "Invalid request";
      case RtspErrc::unauthorized: return "Unauthorized (401)";
      case RtspErrc::forbidden: return "Forbidden (403)";
      case RtspErrc::not_found: return "Not found (404)";
      case RtspErrc::method_not_allowed: return "Method not allowed (405)";
      case RtspErrc::session_not_found: return "Session not found (454)";
      case RtspErrc::unsupported_transport: return "Unsupported transport (461)";
      case RtspErrc::server_error: return "Internal server error (500)";
      case RtspErrc::service_unavailable: return "Service unavailable (503)";
      case RtspErrc::invalid_state: return "Invalid session state";
      case RtspErrc::session_expired: return "Session expired";
      case RtspErrc::setup_required: return "SETUP required";
      case RtspErrc::transport_error: return "Transport error";
      case RtspErrc::invalid_transport: return "Invalid transport";
      case RtspErrc::sdp_parse_error: return "SDP parse error";
      case RtspErrc::invalid_url: return "Invalid RTSP URL";
      case RtspErrc::unknown_error: return "Unknown RTSP error";
      default: return "Unknown RTSP error";
    }
  }
};

inline const RtspErrorCategory&
rtsp_category()
{
  static RtspErrorCategory instance;
  return instance;
}

inline std::error_code
make_error_code(RtspErrc e)
{
  return {static_cast<int>(e), rtsp_category()};
}

// RTSP 상태 코드 -> RtspErrc 변환
inline RtspErrc
rtsp_status_to_errc(uint16_t status_code)
{
  if (status_code >= 200 && status_code < 300) {
    return RtspErrc::success;
  }

  switch (status_code) {
    case 401: return RtspErrc::unauthorized;
    case 403: return RtspErrc::forbidden;
    case 404: return RtspErrc::not_found;
    case 405: return RtspErrc::method_not_allowed;
    case 454: return RtspErrc::session_not_found;
    case 461: return RtspErrc::unsupported_transport;
    case 500: return RtspErrc::server_error;
    case 503: return RtspErrc::service_unavailable;
    default:
      if (status_code >= 400 && status_code < 500) {
        return RtspErrc::invalid_request;
      }
      if (status_code >= 500) {
        return RtspErrc::server_error;
      }
      return RtspErrc::unknown_error;
  }
}

} // namespace nx::net

namespace std {
template <>
struct is_error_code_enum<nx::net::RtspErrc> : true_type
{};
} // namespace std
