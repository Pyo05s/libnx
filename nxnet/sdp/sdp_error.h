// 파일: sdp_error.h
// 생성일: 2026-02-23
// 설명: SDP 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx::sdp {

// SDP 오류 코드
enum class SdpErrc
{
  success = 0,

  // 파싱 오류
  invalid_format = 1,       // 잘못된 SDP 형식
  missing_version = 2,      // 버전 필드 누락
  missing_origin = 3,       // 원본 필드 누락
  missing_session_name = 4, // 세션 이름 누락
  invalid_media_line = 5,   // 잘못된 미디어 라인
  invalid_attribute = 6,    // 잘못된 속성
  invalid_connection = 7,   // 잘못된 연결 정보
  invalid_timing = 8,       // 잘못된 타이밍 정보
  unsupported_version = 9,  // 지원하지 않는 SDP 버전

  // 생성 오류
  no_media = 20, // 미디어 정보 없음

  // 기타
  unknown_error = 99
};

// SDP 오류 카테고리
class SdpErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::sdp"; }

  std::string message(int ev) const override
  {
    switch (static_cast<SdpErrc>(ev)) {
      case SdpErrc::success: return "Success";
      case SdpErrc::invalid_format: return "Invalid SDP format";
      case SdpErrc::missing_version: return "Missing version field";
      case SdpErrc::missing_origin: return "Missing origin field";
      case SdpErrc::missing_session_name: return "Missing session name";
      case SdpErrc::invalid_media_line: return "Invalid media line";
      case SdpErrc::invalid_attribute: return "Invalid attribute";
      case SdpErrc::invalid_connection: return "Invalid connection info";
      case SdpErrc::invalid_timing: return "Invalid timing info";
      case SdpErrc::unsupported_version: return "Unsupported SDP version";
      case SdpErrc::no_media: return "No media description";
      case SdpErrc::unknown_error: return "Unknown SDP error";
      default: return "Unknown SDP error";
    }
  }
};

inline const SdpErrorCategory&
sdp_category()
{
  static SdpErrorCategory instance;
  return instance;
}

inline std::error_code
make_error_code(SdpErrc e)
{
  return {static_cast<int>(e), sdp_category()};
}

} // namespace nx::sdp

namespace std {
template <>
struct is_error_code_enum<nx::sdp::SdpErrc> : true_type
{};
} // namespace std
