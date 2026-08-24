// 파일: onvif_client.cpp
// 생성일: 2025-02-19
// 설명: ONVIF Profile S 클라이언트 통합 구현

#include "onvif_client.h"
#include "nxnet/auth/auth_types.h"
#include "nxnet/auth/ws_security/ws_security_provider.h"
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/use_awaitable.hpp>
#include <nxcore/util/uri_util.h>


namespace nx::net::onvif {

// 기본 Device Service URL
namespace {
constexpr std::string_view kDefaultDeviceServicePath = "/onvif/device_service";
}

OnvifClient::OnvifClient(
  AsioContext& io_context,
  std::string host,
  int port,
  std::string username,
  std::string password)
    : m_io_context(io_context)
    , m_host(std::move(host))
    , m_port(port)
    , m_username(std::move(username))
    , m_password(std::move(password))
{
  // WS-Security 인증 제공자 생성 (Password Digest)
  auth::Credentials creds{m_username, m_password};
  auto auth_result = auth::WsSecurityProvider::create_with_digest(creds);

  std::unique_ptr<auth::AuthProvider> auth_provider;
  if (auth_result.has_value()) {
    auth_provider = std::move(*auth_result);
  }

  // SOAP 클라이언트 생성 (모든 서비스가 공유)
  m_soap_client =
    std::make_shared<soap::SoapClient>(m_io_context, std::move(auth_provider));

  // Device Service 미리 생성 (initialize() 호출 전 connect 단계에서 사용)
  m_device_service = std::make_unique<services::DeviceService>(
    m_soap_client, std::string(kDefaultDeviceServicePath));
}

OnvifClient::~OnvifClient() = default;

nx::awaitable_expected<void>
OnvifClient::check_connection()
{
  // TCP 연결
  auto connect_ec =
    co_await m_soap_client->connect(m_host, static_cast<uint16_t>(m_port));
  if (connect_ec) {
    co_return std::unexpected(connect_ec);
  }

  // GetSystemDateAndTime - 인증 불필요, 접속 확인용으로 충분
  auto time_result = co_await m_device_service->get_system_date_and_time();
  if (!time_result) {
    co_return std::unexpected(time_result.error());
  }

  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::initialize()
{
  // 1. SOAP 클라이언트 연결
  auto connect_ec =
    co_await m_soap_client->connect(m_host, static_cast<uint16_t>(m_port));
  if (connect_ec) {
    co_return std::unexpected(connect_ec);
  }

  // 2. GetSystemDateAndTime - 시간 동기화
  auto time_result = co_await m_device_service->get_system_date_and_time();
  if (!time_result) {
    co_return std::unexpected(time_result.error());
  }
  m_cached_datetime = *time_result;

  // 3. GetCapabilities - 서비스 URL 탐색
  auto caps_result = co_await m_device_service->get_capabilities();
  if (!caps_result) {
    co_return std::unexpected(caps_result.error());
  }
  m_cached_capabilities = *caps_result;

  // 4. Media Service 초기화
  auto media_init = co_await initialize_media_service();
  if (!media_init) {
    co_return media_init;
  }

  // 5. PTZ Service 초기화 (옵션, 실패해도 계속)
  auto ptz_init = co_await initialize_ptz_service();
  if (!ptz_init) {
    // PTZ 서비스가 없거나 초기화 실패 (고정 카메라 등)
    // 로그에 기록하고 계속 진행
    // NX_LOG_WARNING("PTZ 서비스 초기화 실패: " + ptz_init.error().message());
  }

  m_initialized = true;
  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::initialize_media_service()
{
  if (!m_cached_capabilities.has_value()) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  const auto& caps = *m_cached_capabilities;
  if (caps.media.media_service_url.empty()) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kMediaServiceNotFound));
  }

  // Media Service XAddr 파싱하여 서비스 생성
  auto uri_components = nx::parse_uri(caps.media.media_service_url);
  if (!uri_components) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kInvalidResponse));
  }

  // Media Service 생성 (XAddr의 path와 공유 SoapClient 사용)
  const auto& service_path =
    uri_components->path.empty()
      ? std::string(caps.media.media_service_url) // fallback: 전체 URL
      : uri_components->path;

  m_media_service = std::make_unique<services::MediaService>(m_soap_client, service_path);

  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::initialize_ptz_service()
{
  if (!m_cached_capabilities.has_value()) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  const auto& caps = *m_cached_capabilities;
  if (!caps.ptz.has_value() || caps.ptz->ptz_service_url.empty()) {
    // PTZ 서비스 없음 (고정 카메라 등)
    co_return nx::expected<void>{};
  }

  // PTZ Service XAddr 파싱하여 서비스 생성
  auto ptz_uri = nx::parse_uri(caps.ptz->ptz_service_url);
  if (!ptz_uri) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kInvalidResponse));
  }

  // PTZ Service 생성 (공유 SoapClient + path 사용)
  const auto& ptz_path = ptz_uri->path.empty()
                         ? caps.ptz->ptz_service_url // fallback: 전체 URL
                         : ptz_uri->path;

  m_ptz_service = std::make_unique<services::PtzService>(m_soap_client, ptz_path);

  co_return nx::expected<void>{};
}

// ==================== Device Service API ====================

nx::awaitable_expected<DeviceInfo>
OnvifClient::get_device_information()
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  co_return co_await m_device_service->get_device_information();
}

// ==================== Media Service API ====================

nx::awaitable_expected<std::vector<MediaProfile>>
OnvifClient::get_profiles()
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_media_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  co_return co_await m_media_service->get_profiles();
}

nx::awaitable_expected<StreamUri>
OnvifClient::get_stream_uri(
  const std::string& profile_token,
  std::optional<std::string> replace_host,
  std::optional<int> replace_port)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_media_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  auto stream_result = co_await m_media_service->get_stream_uri(profile_token);
  if (!stream_result) {
    co_return stream_result;
  }

  auto stream_uri = *stream_result;

  // IP/Port 교체가 요청된 경우
  if (replace_host.has_value() || replace_port.has_value()) {
    const auto replaced = nx::replace_uri_authority(
      stream_uri.uri, replace_host.value_or(m_host),
      static_cast<uint16_t>(replace_port.value_or(0)));

    stream_uri.uri = replaced;
  }

  co_return stream_uri;
}

nx::awaitable_expected<StreamUri>
OnvifClient::get_main_stream_uri(
  std::optional<std::string> replace_host, std::optional<int> replace_port)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_media_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  // 1. 모든 프로필 조회
  auto profiles_result = co_await m_media_service->get_profiles();
  if (!profiles_result) {
    co_return std::unexpected(profiles_result.error());
  }

  const auto& profiles = *profiles_result;
  if (profiles.empty()) {
    co_return std::unexpected(nx::net::onvif::make_error_code(OnvifError::kNoProfile));
  }

  // 2. 메인 스트림 프로필 선택 (1080p 선호, H.264 우선)
  auto main_profile = m_media_service->select_main_profile(profiles);
  if (!main_profile.has_value()) {
    co_return std::unexpected(nx::net::onvif::make_error_code(OnvifError::kNoProfile));
  }

  // 3. 선택된 프로필의 스트림 URI 조회
  co_return co_await get_stream_uri(main_profile->token, replace_host, replace_port);
}

nx::awaitable_expected<StreamUri>
OnvifClient::get_second_stream_uri(
  std::optional<std::string> replace_host, std::optional<int> replace_port)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_media_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  // 1. 모든 프로필 조회
  auto profiles_result = co_await m_media_service->get_profiles();
  if (!profiles_result) {
    co_return std::unexpected(profiles_result.error());
  }

  const auto& profiles = *profiles_result;
  if (profiles.empty()) {
    co_return std::unexpected(nx::net::onvif::make_error_code(OnvifError::kNoProfile));
  }

  // 2. 서브 스트림 프로필 선택 (720p 선호, 낮은 비트레이트)
  auto second_profile = m_media_service->select_second_profile(profiles);
  if (!second_profile.has_value()) {
    co_return std::unexpected(nx::net::onvif::make_error_code(OnvifError::kNoProfile));
  }

  // 3. 선택된 프로필의 스트림 URI 조회
  co_return co_await get_stream_uri(second_profile->token, replace_host, replace_port);
}

// ==================== PTZ Service API ====================

nx::awaitable_expected<void>
OnvifClient::continuous_move(
  const std::string& profile_token,
  double pan_tilt_x,
  double pan_tilt_y,
  double zoom,
  int timeout_seconds)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  // PtzVelocity 구성
  services::PtzVelocity velocity{};
  velocity.pan_tilt.x = static_cast<float>(pan_tilt_x);
  velocity.pan_tilt.y = static_cast<float>(pan_tilt_y);
  velocity.zoom.x = static_cast<float>(zoom);

  auto ec = co_await m_ptz_service->continuous_move(
    profile_token, velocity, nx::seconds(timeout_seconds));

  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::stop(const std::string& profile_token, bool pan_tilt, bool zoom)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  const auto stop_type = (pan_tilt && zoom) ? services::PtzStopType::kAll
                       : pan_tilt           ? services::PtzStopType::kPanTilt
                                            : services::PtzStopType::kZoom;

  auto ec = co_await m_ptz_service->stop(profile_token, stop_type);
  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

nx::awaitable_expected<std::string>
OnvifClient::set_preset(
  const std::string& profile_token,
  const std::string& preset_name,
  const std::string& preset_token)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  co_return co_await m_ptz_service->set_preset(profile_token, preset_name, preset_token);
}

nx::awaitable_expected<std::vector<services::PtzPreset>>
OnvifClient::get_presets(const std::string& profile_token)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  co_return co_await m_ptz_service->get_presets(profile_token);
}

nx::awaitable_expected<void>
OnvifClient::goto_preset(
  const std::string& profile_token, const std::string& preset_token)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  auto ec = co_await m_ptz_service->goto_preset(profile_token, preset_token);
  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::remove_preset(
  const std::string& profile_token, const std::string& preset_token)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  auto ec = co_await m_ptz_service->remove_preset(profile_token, preset_token);
  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::move_focus(const std::string& video_source_token, double speed)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  auto ec =
    co_await m_ptz_service->move_focus(video_source_token, static_cast<float>(speed));
  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

nx::awaitable_expected<void>
OnvifClient::move_iris(const std::string& video_source_token, double speed)
{
  if (!m_initialized) {
    co_return std::unexpected(std::make_error_code(std::errc::operation_not_permitted));
  }

  if (!m_ptz_service) {
    co_return std::unexpected(
      nx::net::onvif::make_error_code(OnvifError::kServiceNotFound));
  }

  auto ec =
    co_await m_ptz_service->move_iris(video_source_token, static_cast<float>(speed));
  if (ec) {
    co_return std::unexpected(ec);
  }
  co_return nx::expected<void>{};
}

} // namespace nx::net::onvif
