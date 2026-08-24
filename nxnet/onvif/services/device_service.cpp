// 파일: device_service.cpp
// 생성일: 2026-02-19
// 설명: ONVIF Device Management Service 구현

#include "device_service.h"
#include "nxnet/onvif/soap/soap_envelope.h"
#include "nxnet/onvif/onvif_error.h"
#include <nxcore/util/xml_util.h>
#include <nxcore/util/debug_util.h>

namespace nx::net::onvif::services {

// ============================================================================
// 생성자/소멸자
// ============================================================================

DeviceService::DeviceService(
  std::shared_ptr<soap::SoapClient> soap_client, std::string service_url)
    : m_soap_client(std::move(soap_client))
    , m_service_url(std::move(service_url))
{
  NX_ASSERT(m_soap_client != nullptr);
}

DeviceService::~DeviceService() = default;

// ============================================================================
// ONVIF Device Service 메서드
// ============================================================================

nx::awaitable_expected<DateTime>
DeviceService::get_system_date_and_time()
{
  // GetSystemDateAndTime은 인증이 필요 없으므로 빈 body로 요청
  std::string action = "http://www.onvif.org/ver10/device/wsdl/GetSystemDateAndTime";
  std::string body = soap::create_soap_body(
    "GetSystemDateAndTime",
    "", // 파라미터 없음
    soap::ns::kOnvifDevice);

  // 더미 시간 (인증 불필요하므로 현재 시간 사용)
  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_system_date_and_time_response(response->body);
}

nx::awaitable_expected<OnvifCapabilities>
DeviceService::get_capabilities()
{
  std::string action = "http://www.onvif.org/ver10/device/wsdl/GetCapabilities";

  // Category: All (모든 Capabilities 조회)
  std::string body_content = R"(
        <Category>All</Category>
    )";

  std::string body
    = soap::create_soap_body("GetCapabilities", body_content, soap::ns::kOnvifDevice);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_capabilities_response(response->body);
}

nx::awaitable_expected<DeviceInfo>
DeviceService::get_device_information()
{
  std::string action = "http://www.onvif.org/ver10/device/wsdl/GetDeviceInformation";
  std::string body = soap::create_soap_body(
    "GetDeviceInformation",
    "", // 파라미터 없음
    soap::ns::kOnvifDevice);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_device_information_response(response->body);
}

nx::awaitable_expected<std::vector<std::string>>
DeviceService::get_services(bool include_capability)
{
  std::string action = "http://www.onvif.org/ver10/device/wsdl/GetServices";

  std::string body_content = std::string("<IncludeCapability>")
                             + (include_capability ? "true" : "false")
                             + "</IncludeCapability>";

  std::string body
    = soap::create_soap_body("GetServices", body_content, soap::ns::kOnvifDevice);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  // 간단히 빈 벡터 반환 (향후 확장)
  co_return std::vector<std::string>{};
}

// ========================================================================
// 서비스 URL 관리
// ========================================================================

std::string
DeviceService::get_service_url() const
{
  return m_service_url;
}

void
DeviceService::set_service_url(const std::string& url)
{
  m_service_url = url;
}

// ============================================================================
// 내부 파싱 메서드
// ============================================================================

nx::expected<DateTime>
DeviceService::parse_system_date_and_time_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  // GetSystemDateAndTimeResponse 찾기
  auto response_node
    = doc.select_node("//*[local-name()='GetSystemDateAndTimeResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // SystemDateAndTime/UTCDateTime 또는 LocalDateTime 찾기
  auto utc_node = doc.select_node("//*[local-name()='UTCDateTime']");
  if (!utc_node) {
    // LocalDateTime 시도
    utc_node = doc.select_node("//*[local-name()='LocalDateTime']");
    if (!utc_node) {
      return std::unexpected(make_error_code(OnvifError::kInvalidDateTime));
    }
  }

  // Date 파싱
  auto date_node = nx::find_child_ignore_ns(utc_node.value(), "Date");
  auto time_node = nx::find_child_ignore_ns(utc_node.value(), "Time");

  if (!date_node || !time_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidDateTime));
  }

  DateTime dt;

  // Date 필드
  auto year = nx::get_child_int(*date_node, "Year");
  auto month = nx::get_child_int(*date_node, "Month");
  auto day = nx::get_child_int(*date_node, "Day");

  // Time 필드
  auto hour = nx::get_child_int(*time_node, "Hour");
  auto minute = nx::get_child_int(*time_node, "Minute");
  auto second = nx::get_child_int(*time_node, "Second");

  if (!year || !month || !day || !hour || !minute || !second) {
    return std::unexpected(make_error_code(OnvifError::kInvalidDateTime));
  }

  dt.year = *year;
  dt.month = *month;
  dt.day = *day;
  dt.hour = *hour;
  dt.minute = *minute;
  dt.second = *second;
  dt.tz_hour = 0; // UTC
  dt.tz_minute = 0;

  return dt;
}

nx::expected<OnvifCapabilities>
DeviceService::parse_capabilities_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  OnvifCapabilities capabilities;

  // Capabilities 노드 찾기
  auto caps_node = doc.select_node("//*[local-name()='Capabilities']");
  if (!caps_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // Device Capabilities
  auto device_caps = nx::find_child_ignore_ns(caps_node.value(), "Device");
  if (device_caps) {
    auto xaddr = nx::get_child_text(*device_caps, "XAddr");
    if (xaddr) {
      capabilities.device.device_service_url = *xaddr;
    }
  }

  // Media Capabilities
  auto media_caps = nx::find_child_ignore_ns(caps_node.value(), "Media");
  if (media_caps) {
    auto xaddr = nx::get_child_text(*media_caps, "XAddr");
    if (xaddr) {
      capabilities.media.media_service_url = *xaddr;
    }

    // StreamingCapabilities는 bool 값이 아닌 컨테이너 요소이므로 노드 존재
    // 여부로 판단 스트리밍 전송 방식이 필요하면 아래 내용 파싱
    // <tt:StreamingCapabilities>
    //   <tt:RTPMulticast>true</tt:RTPMulticast>
    //  <tt:RTP_TCP>true</tt:RTP_TCP>
    //   <tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>
    // </tt:StreamingCapabilities>
    auto streaming_caps = nx::find_child_ignore_ns(*media_caps, "StreamingCapabilities");
    if (streaming_caps) {
      capabilities.media.streaming_supported = true;
    }
  }

  // PTZ Capabilities (optional)
  auto ptz_caps = nx::find_child_ignore_ns(caps_node.value(), "PTZ");
  if (ptz_caps) {
    PtzCapabilities ptz;
    auto xaddr = nx::get_child_text(*ptz_caps, "XAddr");
    if (xaddr) {
      ptz.ptz_service_url = *xaddr;
      capabilities.ptz = ptz;
    }
  }

  // Events Capabilities (optional)
  auto events_caps = nx::find_child_ignore_ns(caps_node.value(), "Events");
  if (events_caps) {
    EventCapabilities events;
    auto xaddr = nx::get_child_text(*events_caps, "XAddr");
    if (xaddr) {
      events.event_service_url = *xaddr;
    }
    auto pull_point = nx::get_child_bool(*events_caps, "WSPullPointSupport");
    if (pull_point) {
      events.pull_point_supported = *pull_point;
    }
    if (!events.event_service_url.empty()) {
      capabilities.events = events;
    }
  }

  // Imaging Capabilities (optional)
  auto imaging_caps = nx::find_child_ignore_ns(caps_node.value(), "Imaging");
  if (imaging_caps) {
    ImagingCapabilities imaging;
    auto xaddr = nx::get_child_text(*imaging_caps, "XAddr");
    if (xaddr) {
      imaging.imaging_service_url = *xaddr;
      capabilities.imaging = imaging;
    }
  }

  return capabilities;
}

nx::expected<DeviceInfo>
DeviceService::parse_device_information_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  // GetDeviceInformationResponse 찾기
  auto response_node
    = doc.select_node("//*[local-name()='GetDeviceInformationResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  DeviceInfo info;

  // 각 필드 파싱
  auto manufacturer = nx::get_child_text(response_node.value(), "Manufacturer");
  if (manufacturer) {
    info.manufacturer = *manufacturer;
  }

  auto model = nx::get_child_text(response_node.value(), "Model");
  if (model) {
    info.model = *model;
  }

  auto firmware = nx::get_child_text(response_node.value(), "FirmwareVersion");
  if (firmware) {
    info.firmware_version = *firmware;
  }

  auto serial = nx::get_child_text(response_node.value(), "SerialNumber");
  if (serial) {
    info.serial_number = *serial;
  }

  auto hardware = nx::get_child_text(response_node.value(), "HardwareId");
  if (hardware) {
    info.hardware_id = *hardware;
  }

  return info;
}

} // namespace nx::net::onvif::services
