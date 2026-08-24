// 파일: onvif_error.cpp
// 생성일: 2026-02-17
// 설명: ONVIF 프로토콜 에러 코드 구현

#include "onvif_error.h"

namespace nx::net::onvif {

// ============================================================================
// OnvifErrorCategory 구현
// ============================================================================

const char*
OnvifErrorCategory::name() const noexcept
{
  return "onvif";
}

std::string
OnvifErrorCategory::message(int ev) const
{
  switch (static_cast<OnvifError>(ev)) {
    case OnvifError::kSuccess:
      return "Success";

      // SOAP 레벨 에러
    case OnvifError::kSoapParseError: return "Failed to parse SOAP response";
    case OnvifError::kSoapFault: return "SOAP Fault received";
    case OnvifError::kSoapVersionMismatch: return "SOAP version mismatch";
    case OnvifError::kSoapMustUnderstand:
      return "SOAP MustUnderstand header not processed";
    case OnvifError::kSoapInvalidMessage:
      return "Invalid SOAP message";

      // ONVIF 프로토콜 에러
    case OnvifError::kNotAuthorized:
      return "Authentication failed (401 Unauthorized)";
    case OnvifError::kActionNotSupported: return "Action not supported";
    case OnvifError::kOperationProhibited: return "Operation prohibited";
    case OnvifError::kInvalidArguments: return "Invalid arguments";
    case OnvifError::kInvalidToken: return "Invalid token";
    case OnvifError::kInvalidDateTime: return "Invalid date/time format";
    case OnvifError::kInvalidTimezone: return "Invalid timezone";
    case OnvifError::kMaxNVTChannels: return "Maximum NVT channels exceeded";
    case OnvifError::kNoProfile: return "No profile available";
    case OnvifError::kNoVideoSource: return "No video source available";
    case OnvifError::kNoAudioSource: return "No audio source available";
    case OnvifError::kNoMetadata: return "No metadata available";
    case OnvifError::kNoPTZProfile:
      return "No PTZ profile available";

      // 서비스 발견 에러
    case OnvifError::kServiceNotFound: return "Service not found";
    case OnvifError::kDeviceServiceNotFound: return "Device service not found";
    case OnvifError::kMediaServiceNotFound: return "Media service not found";
    case OnvifError::kPtzServiceNotFound: return "PTZ service not found";
    case OnvifError::kEventServiceNotFound:
      return "Event service not found";

      // 네트워크 에러
    case OnvifError::kNetworkError: return "Network error";
    case OnvifError::kConnectionFailed: return "Connection failed";
    case OnvifError::kTimeout: return "Operation timeout";
    case OnvifError::kInvalidResponse:
      return "Invalid response from device";

      // 내부 에러
    case OnvifError::kInternalError: return "Internal error";
    case OnvifError::kNotImplemented: return "Feature not implemented";
    case OnvifError::kInvalidState: return "Invalid state";

    default: return "Unknown ONVIF error";
  }
}

// ============================================================================
// 에러 카테고리 싱글톤
// ============================================================================

const std::error_category&
onvif_category() noexcept
{
  static OnvifErrorCategory instance;
  return instance;
}

// ============================================================================
// make_error_code
// ============================================================================

std::error_code
make_error_code(OnvifError e) noexcept
{
  return {static_cast<int>(e), onvif_category()};
}

} // namespace nx::net::onvif
