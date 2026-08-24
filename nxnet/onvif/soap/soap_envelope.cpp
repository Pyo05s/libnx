// 파일: soap_envelope.cpp
// 생성일: 2026-02-17
// 설명: SOAP Envelope 생성 및 파싱 구현

#include "soap_envelope.h"
#include "nxnet/onvif/onvif_error.h"
#include <nxcore/util/xml_util.h>
#include <pugixml.hpp>
#include <sstream>
#include <random>
#include <iomanip>

namespace nx::net::onvif::soap {

namespace {

// UUID v4 생성 (간략화 버전)
std::string
generate_uuid()
{
  std::random_device rd;
  std::mt19937_64 gen(rd());
  std::uniform_int_distribution<uint64_t> dis;

  uint64_t part1 = dis(gen);
  uint64_t part2 = dis(gen);

  std::ostringstream oss;
  oss << std::hex << std::setfill('0') << std::setw(8) << (part1 >> 32) << "-"
      << std::setw(4) << ((part1 >> 16) & 0xFFFF) << "-" << std::setw(4)
      << (part1 & 0xFFFF) << "-" << std::setw(4) << (part2 >> 48) << "-" << std::setw(12)
      << (part2 & 0xFFFFFFFFFFFF);

  return oss.str();
}

} // anonymous namespace

// ============================================================================
// SOAP Envelope 생성
// ============================================================================

nx::expected<std::string>
create_soap_envelope(const SoapRequest& request, const DateTime&)
{
  pugi::xml_document doc;

  // SOAP Envelope
  pugi::xml_node envelope = doc.append_child("s:Envelope");
  envelope.append_attribute("xmlns:s") = ns::kSoapEnv;
  envelope.append_attribute("xmlns:a") = ns::kWsAddressing;

  // SOAP Header
  pugi::xml_node header = envelope.append_child("s:Header");

  // WS-Addressing
  std::string msg_id = generate_message_id();
  pugi::xml_node action_node = header.append_child("a:Action");
  action_node.append_attribute("s:mustUnderstand") = "1";
  action_node.text().set(request.action.c_str());

  pugi::xml_node msg_id_node = header.append_child("a:MessageID");
  msg_id_node.text().set(msg_id.c_str());

  pugi::xml_node reply_to = header.append_child("a:ReplyTo");
  pugi::xml_node address = reply_to.append_child("a:Address");
  address.text().set("http://www.w3.org/2005/08/addressing/anonymous");

  pugi::xml_node to_node = header.append_child("a:To");
  to_node.append_attribute("s:mustUnderstand") = "1";
  to_node.text().set("http://onvif/device_service");

  // WS-Security 헤더 추가 (제공된 경우)
  if (request.security_header.has_value()) {
    pugi::xml_document sec_doc;
    if (sec_doc.load_string(request.security_header->c_str())) {
      header.append_copy(sec_doc.document_element());
    }
  }

  // 커스텀 헤더 추가
  for (const auto& [name, value] : request.custom_headers) {
    pugi::xml_node custom = header.append_child(name.c_str());
    custom.text().set(value.c_str());
  }

  // SOAP Body
  pugi::xml_node body = envelope.append_child("s:Body");

  // Body 내용 추가
  if (!request.body.empty()) {
    pugi::xml_document body_doc;
    if (body_doc.load_string(request.body.c_str())) {
      body.append_copy(body_doc.document_element());
    }
    else {
      return std::unexpected(make_error_code(OnvifError::kSoapInvalidMessage));
    }
  }

  // XML 문자열로 변환
  std::ostringstream oss;
  doc.save(
    oss,
    "  ",
    pugi::format_default | pugi::format_no_declaration,
    pugi::encoding_utf8);

  return oss.str();
}

std::string
create_soap_body(
  const std::string& action,
  const std::string& body_content,
  const std::string& target_namespace)
{
  pugi::xml_document doc;
  pugi::xml_node root = doc.append_child(action.c_str());
  root.append_attribute("xmlns") = target_namespace.c_str();

  // Body 내용 추가
  if (!body_content.empty()) {
    pugi::xml_document content_doc;
    // parse_fragment: 다중 루트 엘리먼트 지원 (예:
    // <StreamSetup/><ProfileToken/>)
    auto parse_result = content_doc.load_string(
      body_content.c_str(),
      pugi::parse_default | pugi::parse_fragment);
    if (parse_result) {
      for (pugi::xml_node child : content_doc.children()) {
        root.append_copy(child);
      }
    }
  }

  std::ostringstream oss;
  doc.save(
    oss,
    "  ",
    pugi::format_default | pugi::format_no_declaration,
    pugi::encoding_utf8);

  return oss.str();
}

// ============================================================================
// WS-Addressing 헤더 생성
// ============================================================================

std::string
create_ws_addressing_header(const std::string& action, const std::string& message_id)
{
  pugi::xml_document doc;

  pugi::xml_node action_node = doc.append_child("a:Action");
  action_node.append_attribute("xmlns:a") = ns::kWsAddressing;
  action_node.append_attribute("s:mustUnderstand") = "1";
  action_node.text().set(action.c_str());

  std::string msg_id = message_id.empty() ? generate_message_id() : message_id;
  pugi::xml_node msg_id_node = doc.append_child("a:MessageID");
  msg_id_node.append_attribute("xmlns:a") = ns::kWsAddressing;
  msg_id_node.text().set(msg_id.c_str());

  std::ostringstream oss;
  doc.save(
    oss,
    "  ",
    pugi::format_default | pugi::format_no_declaration,
    pugi::encoding_utf8);

  return oss.str();
}

// ============================================================================
// SOAP 응답 파싱
// ============================================================================
nx::expected<SoapResponse>
parse_soap_response(const std::string& xml_response)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(xml_response);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  SoapResponse response;

  // SOAP Envelope 확인
  auto envelope = doc.select_node("//*[local-name()='Envelope']");
  if (!envelope) {
    return std::unexpected(make_error_code(OnvifError::kSoapInvalidMessage));
  }

  // SOAP Body 찾기
  auto body_node = doc.select_node("//*[local-name()='Body']");
  if (!body_node) {
    return std::unexpected(make_error_code(OnvifError::kSoapInvalidMessage));
  }

  // SOAP Fault 확인
  auto fault_node = doc.select_node("//*[local-name()='Fault']");
  if (fault_node) {
    response.is_fault = true;

    // Fault Code
    auto code_value = nx::get_child_text(fault_node.value(), "Value");
    if (code_value) {
      response.fault_code = nx::strip_namespace(*code_value);
    }

    // Fault Subcode
    auto subcode = nx::find_child_ignore_ns(fault_node.value(), "Subcode");
    if (subcode) {
      auto subcode_value = nx::get_child_text(*subcode, "Value");
      if (subcode_value) {
        response.fault_subcode = nx::strip_namespace(*subcode_value);
      }
    }

    // Fault Reason
    auto reason_text = doc.select_node("//*[local-name()='Text']");
    if (reason_text) {
      response.fault_reason = nx::get_node_text(reason_text.value());
    }

    // Fault Detail
    auto detail_node = doc.select_node("//*[local-name()='Detail']");
    if (detail_node) {
      std::ostringstream oss;
      detail_node.value().print(oss, "  ", pugi::format_default);
      response.fault_detail = oss.str();
    }

    return response;
  }

  // 정상 응답 - Body 내용 추출
  std::ostringstream oss;
  for (pugi::xml_node child : body_node->children()) {
    child.print(oss, "  ", pugi::format_default);
  }
  response.body = oss.str();

  return response;
}

nx::expected<std::string>
extract_body_element(const std::string& soap_body, const std::string& element_name)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(soap_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  // element_name으로 노드 찾기 (네임스페이스 무시)
  std::string xpath = "//*[local-name()='" + element_name + "']";
  auto node = doc.select_node(xpath);
  if (!node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  std::ostringstream oss;
  node.value().print(oss, "  ", pugi::format_default);

  return oss.str();
}

// ============================================================================
// 헬퍼 함수
// ============================================================================

std::string
generate_message_id()
{
  return "urn:uuid:" + generate_uuid();
}

std::string
get_action_namespace(const std::string& action)
{
  // ONVIF Device Service 액션
  if (
    action.find("GetSystemDateAndTime") != std::string::npos
    || action.find("GetDeviceInformation") != std::string::npos
    || action.find("GetCapabilities") != std::string::npos
    || action.find("GetServices") != std::string::npos) {
    return ns::kOnvifDevice;
  }

  // ONVIF Media Service 액션
  if (
    action.find("GetProfiles") != std::string::npos
    || action.find("GetStreamUri") != std::string::npos
    || action.find("GetVideoEncoderConfiguration") != std::string::npos) {
    return ns::kOnvifMedia;
  }

  // ONVIF PTZ Service 액션
  if (
    action.find("PTZ") != std::string::npos
    || action.find("AbsoluteMove") != std::string::npos
    || action.find("RelativeMove") != std::string::npos) {
    return ns::kOnvifPtz;
  }

  // 기본값: Device Service
  return ns::kOnvifDevice;
}

} // namespace nx::net::onvif::soap
