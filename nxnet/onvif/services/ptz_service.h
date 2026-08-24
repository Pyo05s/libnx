// 파일: ptz_service.h
// 생성일: 2026-02-19
// 설명: ONVIF PTZ Service (Continuous 방식 제어)

#pragma once

#include "nxnet/onvif/soap/soap_client.h"
#include "nxnet/onvif/onvif_types.h"
#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <expected>
#include <memory>
#include <string>
#include <vector>

namespace nx::net::onvif::services {

// ============================================================================
// PTZ 벡터 및 속도
// ============================================================================

/// PTZ 2차원 벡터 (Pan/Tilt 또는 x/y 좌표)
struct PtzVector2D
{
  float x{0.0f}; // -1.0 ~ 1.0 (왼쪽/오른쪽 또는 Pan)
  float y{0.0f}; // -1.0 ~ 1.0 (아래/위 또는 Tilt)
};

/// PTZ 1차원 벡터 (Zoom)
struct PtzVector1D
{
  float x{0.0f}; // -1.0 ~ 1.0 (Zoom Out/In)
};

/// PTZ 속도 (Continuous Move용)
struct PtzVelocity
{
  PtzVector2D pan_tilt; // Pan/Tilt 속도
  PtzVector1D zoom;     // Zoom 속도
};

// ============================================================================
// PTZ Preset
// ============================================================================

/// PTZ Preset 정보
struct PtzPreset
{
  std::string token; // Preset 토큰
  std::string name;  // Preset 이름
};

// ============================================================================
// PTZ 제어 옵션
// ============================================================================

/// 정지할 이동 타입
enum class PtzStopType
{
  kAll,     // 모든 이동 중지
  kPanTilt, // Pan/Tilt만 중지
  kZoom     // Zoom만 중지
};

// ============================================================================
// PTZ Service
// ============================================================================

/// ONVIF PTZ Service (tptz)
/// Pan/Tilt/Zoom 제어 및 Preset 관리 (Continuous 방식)
class PtzService
{
public:
  /// 생성자
  /// @param soap_client SOAP 클라이언트 (공유)
  /// @param service_url PTZ Service URL
  explicit PtzService(
    std::shared_ptr<soap::SoapClient> soap_client, std::string service_url);

  ~PtzService();

  NX_NON_COPYABLE_AND_MOVABLE(PtzService);

  // ========================================================================
  // PTZ 연속 이동 제어
  // ========================================================================

  /// ContinuousMove
  /// Pan/Tilt/Zoom 연속 이동 (속도 기반)
  /// @param profile_token 프로파일 토큰
  /// @param velocity PTZ 속도 (-1.0 ~ 1.0)
  /// @param timeout 이동 지속 시간 (0 = 무한, Stop 호출 시까지)
  nx::awaitable<std::error_code> continuous_move(
    const std::string& profile_token,
    const PtzVelocity& velocity,
    nx::seconds timeout = nx::seconds{0});

  /// Stop
  /// PTZ 이동 중지
  /// @param profile_token 프로파일 토큰
  /// @param stop_type 중지할 이동 타입 (All, PanTilt, Zoom)
  nx::awaitable<std::error_code>
  stop(const std::string& profile_token, PtzStopType stop_type = PtzStopType::kAll);

  // ========================================================================
  // 간편 제어 메서드
  // ========================================================================

  /// Pan/Tilt 이동
  /// @param profile_token 프로파일 토큰
  /// @param pan_speed Pan 속도 (-1.0: 왼쪽, 1.0: 오른쪽)
  /// @param tilt_speed Tilt 속도 (-1.0: 아래, 1.0: 위)
  nx::awaitable<std::error_code>
  move_pan_tilt(const std::string& profile_token, float pan_speed, float tilt_speed);

  /// Zoom 제어
  /// @param profile_token 프로파일 토큰
  /// @param zoom_speed Zoom 속도 (-1.0: Zoom Out, 1.0: Zoom In)
  nx::awaitable<std::error_code>
  move_zoom(const std::string& profile_token, float zoom_speed);

  /// Focus 제어 (Continuous)
  /// @param profile_token 프로파일 토큰
  /// @param focus_speed Focus 속도 (-1.0: Far, 1.0: Near)
  nx::awaitable<std::error_code>
  move_focus(const std::string& profile_token, float focus_speed);

  /// Iris 제어 (Continuous)
  /// @param profile_token 프로파일 토큰
  /// @param iris_speed Iris 속도 (-1.0: Close, 1.0: Open)
  nx::awaitable<std::error_code>
  move_iris(const std::string& profile_token, float iris_speed);

  // ========================================================================
  // PTZ Preset 관리
  // ========================================================================

  /// SetPreset
  /// 현재 PTZ 위치를 Preset으로 저장
  /// @param profile_token 프로파일 토큰
  /// @param preset_name Preset 이름
  /// @param preset_token Preset 토큰 (비어있으면 자동 생성)
  /// @return 생성된 Preset 토큰
  nx::awaitable_expected<std::string> set_preset(
    const std::string& profile_token,
    const std::string& preset_name,
    const std::string& preset_token = {});

  /// GetPresets
  /// Preset 목록 조회
  /// @param profile_token 프로파일 토큰
  /// @return Preset 목록
  nx::awaitable_expected<std::vector<PtzPreset>>
  get_presets(const std::string& profile_token);

  /// GotoPreset
  /// Preset 위치로 이동
  /// @param profile_token 프로파일 토큰
  /// @param preset_token Preset 토큰
  nx::awaitable<std::error_code>
  goto_preset(const std::string& profile_token, const std::string& preset_token);

  /// RemovePreset
  /// Preset 삭제
  /// @param profile_token 프로파일 토큰
  /// @param preset_token Preset 토큰
  nx::awaitable<std::error_code>
  remove_preset(const std::string& profile_token, const std::string& preset_token);

  // ========================================================================
  // 서비스 URL 관리
  // ========================================================================

  /// 서비스 URL 조회
  std::string get_service_url() const;

  /// 서비스 URL 변경
  void set_service_url(const std::string& url);

private:
  // ========================================================================
  // 내부 파싱 메서드
  // ========================================================================

  /// SetPreset 응답 파싱 (Preset 토큰 추출)
  nx::expected<std::string> parse_set_preset_response(const std::string& response_body);

  /// GetPresets 응답 파싱
  nx::expected<std::vector<PtzPreset>>
  parse_presets_response(const std::string& response_body);

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  std::shared_ptr<soap::SoapClient> m_soap_client;
  std::string m_service_url;
};

} // namespace nx::net::onvif::services
