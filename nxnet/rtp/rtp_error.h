// 파일: rtp_error.h
// 생성일: 2026-02-23
// 설명: RTP 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx::rtp {

// RTP 오류 코드
enum class RtpErrc
{
  success = 0,

  // 패킷 파싱 오류
  packet_too_short = 1, // 패킷이 너무 짧음
  invalid_version = 2,  // 잘못된 RTP 버전
  invalid_header = 3,   // 잘못된 헤더
  invalid_payload = 4,  // 잘못된 페이로드

  // 수신 오류
  bind_failed = 10,    // 소켓 바인딩 실패
  receive_failed = 11, // 수신 실패
  socket_closed = 12,  // 소켓 닫힘
  not_started = 13,    // 수신 시작되지 않음

  // 디패킷타이징 오류
  fragmentation_error = 20, // 조각화 오류
  sequence_gap = 21,        // 시퀀스 갭 발생

  // RTCP 오류
  invalid_rtcp = 30, // 잘못된 RTCP 패킷

  // 기타
  unknown_error = 99
};

class RtpErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::rtp"; }

  std::string message(int ev) const override
  {
    switch (static_cast<RtpErrc>(ev)) {
      case RtpErrc::success: return "Success";
      case RtpErrc::packet_too_short: return "Packet too short";
      case RtpErrc::invalid_version: return "Invalid RTP version";
      case RtpErrc::invalid_header: return "Invalid RTP header";
      case RtpErrc::invalid_payload: return "Invalid payload";
      case RtpErrc::bind_failed: return "Socket bind failed";
      case RtpErrc::receive_failed: return "Receive failed";
      case RtpErrc::socket_closed: return "Socket closed";
      case RtpErrc::not_started: return "Receiver not started";
      case RtpErrc::fragmentation_error: return "Fragmentation error";
      case RtpErrc::sequence_gap: return "Sequence gap detected";
      case RtpErrc::invalid_rtcp: return "Invalid RTCP packet";
      case RtpErrc::unknown_error: return "Unknown RTP error";
      default: return "Unknown RTP error";
    }
  }
};

inline const RtpErrorCategory&
rtp_category()
{
  static RtpErrorCategory instance;
  return instance;
}

inline std::error_code
make_error_code(RtpErrc e)
{
  return {static_cast<int>(e), rtp_category()};
}

} // namespace nx::rtp

namespace std {
template <>
struct is_error_code_enum<nx::rtp::RtpErrc> : true_type
{};
} // namespace std
