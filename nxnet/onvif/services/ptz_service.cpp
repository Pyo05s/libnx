// 파일: ptz_service.cpp
// 생성일: 2026-02-19
// 설명: ONVIF PTZ Service 구현 (Continuous 방식 제어)

#include "ptz_service.h"
#include "nxnet/onvif/soap/soap_envelope.h"
#include "nxnet/onvif/onvif_error.h"
#include <nxcore/util/xml_util.h>
#include <nxcore/util/debug_util.h>

#include <sstream>
#include <iomanip>

namespace nx::net::onvif::services {

// ============================================================================
// 생성자/소멸자
// ============================================================================

PtzService::PtzService(
  std::shared_ptr<soap::SoapClient> soap_client, std::string service_url)
    : m_soap_client(std::move(soap_client))
    , m_service_url(std::move(service_url))
{
  NX_ASSERT(m_soap_client != nullptr);
}

PtzService::~PtzService() = default;

// ============================================================================
// PTZ 연속 이동 제어
// ============================================================================

nx::awaitable<std::error_code>
PtzService::continuous_move(
  const std::string& profile_token, const PtzVelocity& velocity, nx::seconds timeout)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/ContinuousMove";

  // Velocity 내부 요소(PanTilt, Zoom)는 tt: 네임스페이스에 속함
  std::string schema_ns = soap::ns::kOnvifSchema;
  std::ostringstream body_stream;
  body_stream << "<ProfileToken>" << profile_token << "</ProfileToken>";
  body_stream << "<Velocity>";
  body_stream << "<PanTilt xmlns=\"" << schema_ns << "\" x=\"" << velocity.pan_tilt.x
              << "\" y=\"" << velocity.pan_tilt.y
              << "\" "
                 "space=\"http://www.onvif.org/ver10/tptz/PanTiltSpaces/"
                 "VelocityGenericSpace\"/>";
  body_stream << "<Zoom xmlns=\"" << schema_ns << "\" x=\"" << velocity.zoom.x
              << "\" "
                 "space=\"http://www.onvif.org/ver10/tptz/ZoomSpaces/"
                 "VelocityGenericSpace\"/>";
  body_stream << "</Velocity>";

  // Timeout 추가 (0이 아닌 경우)
  if (timeout.count() > 0) {
    body_stream << "<Timeout>PT" << timeout.count() << "S</Timeout>";
  }

  std::string body
    = soap::create_soap_body("ContinuousMove", body_stream.str(), soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

nx::awaitable<std::error_code>
PtzService::stop(const std::string& profile_token, PtzStopType stop_type)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/Stop";

  // Stop 타입에 따라 PanTilt/Zoom 플래그 설정
  bool stop_pan_tilt
    = (stop_type == PtzStopType::kAll || stop_type == PtzStopType::kPanTilt);
  bool stop_zoom = (stop_type == PtzStopType::kAll || stop_type == PtzStopType::kZoom);

  std::ostringstream body_stream;
  body_stream << "<ProfileToken>" << profile_token << "</ProfileToken>";
  body_stream << "<PanTilt>" << (stop_pan_tilt ? "true" : "false") << "</PanTilt>";
  body_stream << "<Zoom>" << (stop_zoom ? "true" : "false") << "</Zoom>";

  std::string body
    = soap::create_soap_body("Stop", body_stream.str(), soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

// ========================================================================
// 간편 제어 메서드
// ========================================================================

nx::awaitable<std::error_code>
PtzService::move_pan_tilt(
  const std::string& profile_token, float pan_speed, float tilt_speed)
{
  PtzVelocity velocity;
  velocity.pan_tilt.x = pan_speed;
  velocity.pan_tilt.y = tilt_speed;
  velocity.zoom.x = 0.0f;

  return continuous_move(profile_token, velocity);
}

nx::awaitable<std::error_code>
PtzService::move_zoom(const std::string& profile_token, float zoom_speed)
{
  PtzVelocity velocity;
  velocity.pan_tilt.x = 0.0f;
  velocity.pan_tilt.y = 0.0f;
  velocity.zoom.x = zoom_speed;

  return continuous_move(profile_token, velocity);
}

nx::awaitable<std::error_code>
PtzService::move_focus(const std::string& profile_token, float focus_speed)
{
  // Focus는 Imaging Service의 Move 메서드를 사용
  // 간단히 하기 위해 여기서는 기본 구현만 제공
  std::string action = "http://www.onvif.org/ver20/imaging/wsdl/Move";

  std::ostringstream body_stream;
  body_stream << "<VideoSourceToken>" << profile_token << "</VideoSourceToken>";
  body_stream << "<Focus>";
  body_stream << "<Continuous>";
  body_stream << "<Speed>" << focus_speed << "</Speed>";
  body_stream << "</Continuous>";
  body_stream << "</Focus>";

  std::string body
    = soap::create_soap_body("Move", body_stream.str(), soap::ns::kOnvifImaging);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

nx::awaitable<std::error_code>
PtzService::move_iris(const std::string& profile_token, float iris_speed)
{
  // Iris도 Imaging Service의 Move 메서드 사용
  std::string action = "http://www.onvif.org/ver20/imaging/wsdl/Move";

  std::ostringstream body_stream;
  body_stream << "<VideoSourceToken>" << profile_token << "</VideoSourceToken>";
  body_stream << "<IrisAutoAdjustment>";
  body_stream << "<Continuous>";
  body_stream << "<Speed>" << iris_speed << "</Speed>";
  body_stream << "</Continuous>";
  body_stream << "</IrisAutoAdjustment>";

  std::string body
    = soap::create_soap_body("Move", body_stream.str(), soap::ns::kOnvifImaging);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

// ========================================================================
// PTZ Preset 관리
// ========================================================================

nx::awaitable_expected<std::string>
PtzService::set_preset(
  const std::string& profile_token,
  const std::string& preset_name,
  const std::string& preset_token)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/SetPreset";

  std::ostringstream body_stream;
  body_stream << "<ProfileToken>" << profile_token << "</ProfileToken>";

  if (!preset_token.empty()) {
    body_stream << "<PresetToken>" << preset_token << "</PresetToken>";
  }

  if (!preset_name.empty()) {
    body_stream << "<PresetName>" << preset_name << "</PresetName>";
  }

  std::string body
    = soap::create_soap_body("SetPreset", body_stream.str(), soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_set_preset_response(response->body);
}

nx::awaitable_expected<std::vector<PtzPreset>>
PtzService::get_presets(const std::string& profile_token)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/GetPresets";

  std::string body_content = "<ProfileToken>" + profile_token + "</ProfileToken>";

  std::string body
    = soap::create_soap_body("GetPresets", body_content, soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_presets_response(response->body);
}

nx::awaitable<std::error_code>
PtzService::goto_preset(const std::string& profile_token, const std::string& preset_token)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/GotoPreset";

  std::ostringstream body_stream;
  body_stream << "<ProfileToken>" << profile_token << "</ProfileToken>";
  body_stream << "<PresetToken>" << preset_token << "</PresetToken>";

  std::string body
    = soap::create_soap_body("GotoPreset", body_stream.str(), soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

nx::awaitable<std::error_code>
PtzService::remove_preset(
  const std::string& profile_token, const std::string& preset_token)
{
  std::string action = "http://www.onvif.org/ver20/ptz/wsdl/RemovePreset";

  std::ostringstream body_stream;
  body_stream << "<ProfileToken>" << profile_token << "</ProfileToken>";
  body_stream << "<PresetToken>" << preset_token << "</PresetToken>";

  std::string body
    = soap::create_soap_body("RemovePreset", body_stream.str(), soap::ns::kOnvifPtz);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return response.error();
  }

  co_return std::error_code{};
}

// ========================================================================
// 서비스 URL 관리
// ========================================================================

std::string
PtzService::get_service_url() const
{
  return m_service_url;
}

void
PtzService::set_service_url(const std::string& url)
{
  m_service_url = url;
}

// ============================================================================
// 내부 파싱 메서드
// ============================================================================

nx::expected<std::string>
PtzService::parse_set_preset_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  // SetPresetResponse 찾기
  auto response_node = doc.select_node("//*[local-name()='SetPresetResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // PresetToken 추출
  auto preset_token = nx::get_child_text(response_node.value(), "PresetToken");
  if (!preset_token) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  return *preset_token;
}

nx::expected<std::vector<PtzPreset>>
PtzService::parse_presets_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  std::vector<PtzPreset> presets;

  // GetPresetsResponse 찾기
  auto response_node = doc.select_node("//*[local-name()='GetPresetsResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // Preset 노드들 찾기
  auto preset_nodes = doc.select_nodes("//*[local-name()='Preset']");

  for (const auto& preset_node : preset_nodes) {
    PtzPreset preset;

    // Token 속성
    preset.token = nx::get_node_attribute(preset_node.node(), "token");

    // Name
    auto name = nx::get_child_text(preset_node.node(), "Name");
    if (name) {
      preset.name = *name;
    }

    presets.push_back(std::move(preset));
  }

  return presets;
}

} // namespace nx::net::onvif::services
