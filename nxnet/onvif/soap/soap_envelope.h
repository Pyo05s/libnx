// 파일: soap_envelope.h
// 생성일: 2026-02-17
// 설명: SOAP Envelope 생성 및 파싱

#pragma once

#include "soap_types.h"
#include "nxnet/onvif/onvif_types.h"

#include <nxcore/util/type_util.h>

#include <expected>
#include <system_error>
#include <string>

namespace nx::net::onvif::soap {

// ============================================================================
// SOAP Envelope 생성
// ============================================================================

/// SOAP 1.2 Envelope 생성
/// @param request SOAP 요청 메시지
/// @param camera_time 카메라 시간 (WS-Security Created 타임스탬프용)
/// @return SOAP Envelope XML 문자열
nx::expected<std::string>
create_soap_envelope(const SoapRequest& request, const DateTime& camera_time);

/// SOAP Body만 생성 (Envelope 없이)
/// @param action Action 이름
/// @param body_content Body 내용
/// @param target_namespace 대상 네임스페이스
/// @return SOAP Body XML 문자열
std::string create_soap_body(
  const std::string& action,
  const std::string& body_content,
  const std::string& target_namespace);

// ============================================================================
// WS-Addressing 헤더 생성
// ============================================================================

/// WS-Addressing 헤더 생성
/// @param action Action URI
/// @param message_id 메시지 ID (선택 사항, 비어 있으면 자동 생성)
/// @return WS-Addressing 헤더 XML
std::string create_ws_addressing_header(
  const std::string& action, const std::string& message_id = {});

// ============================================================================
// SOAP 응답 파싱
// ============================================================================

/// SOAP Envelope 파싱
/// @param xml_response SOAP 응답 XML 문자열
/// @return 파싱된 SOAP 응답
nx::expected<SoapResponse> parse_soap_response(const std::string& xml_response);

/// SOAP Body에서 특정 요소 추출
/// @param soap_body SOAP Body XML
/// @param element_name 추출할 요소 이름 (네임스페이스 무시)
/// @return 추출된 요소 (pugixml 노드)
nx::expected<std::string>
extract_body_element(const std::string& soap_body, const std::string& element_name);

// ============================================================================
// 헬퍼 함수
// ============================================================================

/// UUID 생성 (메시지 ID용)
std::string generate_message_id();

/// ONVIF Action에서 네임스페이스 URI 추출
/// 예: "GetDeviceInformation" -> "http://www.onvif.org/ver10/device/wsdl"
std::string get_action_namespace(const std::string& action);

} // namespace nx::net::onvif::soap
