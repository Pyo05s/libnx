// 파일: onvif_error.h
// 생성일: 2026-02-17
// 설명: ONVIF 프로토콜 에러 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx::net::onvif {

// ============================================================================
// ONVIF 에러 코드 열거형
// ============================================================================

enum class OnvifError
{
  kSuccess = 0,

  // SOAP 레벨 에러
  kSoapParseError = 1,  // SOAP 응답 파싱 실패
  kSoapFault,           // SOAP Fault 수신
  kSoapVersionMismatch, // SOAP 버전 불일치
  kSoapMustUnderstand,  // SOAP MustUnderstand 헤더 처리 실패
  kSoapInvalidMessage,  // 잘못된 SOAP 메시지

  // ONVIF 프로토콜 에러
  kNotAuthorized = 10,  // 인증 실패 (401)
  kActionNotSupported,  // 지원하지 않는 Action
  kOperationProhibited, // 작업 금지
  kInvalidArguments,    // 잘못된 인자
  kInvalidToken,        // 잘못된 토큰
  kInvalidDateTime,     // 잘못된 날짜/시간
  kInvalidTimezone,     // 잘못된 시간대
  kMaxNVTChannels,      // 최대 채널 초과
  kNoProfile,           // 프로파일 없음
  kNoVideoSource,       // 비디오 소스 없음
  kNoAudioSource,       // 오디오 소스 없음
  kNoMetadata,          // 메타데이터 없음
  kNoPTZProfile,        // PTZ 프로파일 없음

  // 서비스 발견 에러
  kServiceNotFound = 30,  // 서비스 주소를 찾을 수 없음
  kDeviceServiceNotFound, // Device Service 없음
  kMediaServiceNotFound,  // Media Service 없음
  kPtzServiceNotFound,    // PTZ Service 없음
  kEventServiceNotFound,  // Event Service 없음

  // 네트워크 에러
  kNetworkError = 40, // 네트워크 오류
  kConnectionFailed,  // 연결 실패
  kTimeout,           // 타임아웃
  kInvalidResponse,   // 잘못된 응답

  // 내부 에러
  kInternalError = 50, // 내부 오류
  kNotImplemented,     // 미구현 기능
  kInvalidState,       // 잘못된 상태
};

// ============================================================================
// std::error_code 지원
// ============================================================================

// ONVIF 에러 카테고리
class OnvifErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

// 에러 카테고리 싱글톤
const std::error_category& onvif_category() noexcept;

// OnvifError를 std::error_code로 변환
std::error_code make_error_code(OnvifError e) noexcept;

} // namespace nx::net::onvif

// OnvifError를 std::error_code로 자동 변환 가능하게 등록
namespace std {
template <>
struct is_error_code_enum<nx::net::onvif::OnvifError> : true_type
{};
} // namespace std
