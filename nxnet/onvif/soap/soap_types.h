// 파일: soap_types.h
// 생성일: 2026-02-17
// 설명: SOAP 메시지 타입 정의

#pragma once

#include <string>
#include <map>
#include <optional>

namespace nx::net::onvif::soap {

// ============================================================================
// SOAP 네임스페이스 상수
// ============================================================================

namespace ns {

// SOAP 1.2
constexpr const char* kSoapEnv = "http://www.w3.org/2003/05/soap-envelope";

// WS-Addressing
constexpr const char* kWsAddressing = "http://www.w3.org/2005/08/addressing";

// WS-Security
constexpr const char* kWsse = "http://docs.oasis-open.org/wss/2004/01/"
                              "oasis-200401-wss-wssecurity-secext-1.0.xsd";
constexpr const char* kWsu = "http://docs.oasis-open.org/wss/2004/01/"
                             "oasis-200401-wss-wssecurity-utility-1.0.xsd";

// ONVIF 서비스
constexpr const char* kOnvifDevice = "http://www.onvif.org/ver10/device/wsdl";
constexpr const char* kOnvifMedia = "http://www.onvif.org/ver10/media/wsdl";
constexpr const char* kOnvifPtz = "http://www.onvif.org/ver20/ptz/wsdl";
constexpr const char* kOnvifEvent = "http://www.onvif.org/ver10/events/wsdl";
constexpr const char* kOnvifImaging = "http://www.onvif.org/ver20/imaging/wsdl";

// ONVIF 스키마
constexpr const char* kOnvifSchema = "http://www.onvif.org/ver10/schema";

} // namespace ns

// ============================================================================
// SOAP 요청 구조체
// ============================================================================

/// SOAP 요청 메시지
struct SoapRequest
{
  std::string action; // WS-Addressing Action (GetDeviceInformation 등)
  std::string body;   // SOAP Body 내용 (XML)
  std::optional<std::string> security_header;        // WS-Security 헤더 (선택 사항)
  std::map<std::string, std::string> custom_headers; // 추가 헤더
};

// ============================================================================
// SOAP 응답 구조체
// ============================================================================

/// SOAP 응답 메시지
struct SoapResponse
{
  bool is_fault{false}; // SOAP Fault 여부
  std::string body;     // SOAP Body 내용

  // Fault 정보 (is_fault == true일 때 유효)
  std::string fault_code;    // Fault Code (Sender, Receiver 등)
  std::string fault_subcode; // Fault Subcode (ONVIF 에러 코드)
  std::string fault_reason;  // Fault Reason (에러 메시지)
  std::string fault_detail;  // Fault Detail (상세 정보)
};

// ============================================================================
// SOAP Fault 코드
// ============================================================================

namespace fault {

// SOAP 1.2 표준 Fault Code
constexpr const char* kVersionMismatch = "VersionMismatch";
constexpr const char* kMustUnderstand = "MustUnderstand";
constexpr const char* kDataEncodingUnknown = "DataEncodingUnknown";
constexpr const char* kSender = "Sender";     // 클라이언트 측 에러
constexpr const char* kReceiver = "Receiver"; // 서버 측 에러

// ONVIF 특화 Subcode
constexpr const char* kNotAuthorized = "NotAuthorized";
constexpr const char* kActionNotSupported = "ActionNotSupported";
constexpr const char* kInvalidArgVal = "InvalidArgVal";
constexpr const char* kInvalidArgs = "InvalidArgs";
constexpr const char* kOperationProhibited = "OperationProhibited";

} // namespace fault

} // namespace nx::net::onvif::soap
