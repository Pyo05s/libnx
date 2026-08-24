// 파일: onvif_client.h
// 생성일: 2025-02-19
// 설명: ONVIF Profile S 클라이언트 통합 인터페이스

#pragma once

#include "onvif_types.h"
#include "onvif_error.h"
#include "services/device_service.h"
#include "services/media_service.h"
#include "services/ptz_service.h"
#include "soap/soap_client.h"

#include <nxcore/util/type_util.h>
#include <boost/asio/awaitable.hpp>
#include <boost/asio/io_context.hpp>
#include <expected>
#include <memory>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace nx::net::onvif {

/// @brief ONVIF Profile S 클라이언트 통합 클래스
/// @details Device, Media, PTZ 서비스를 통합하여 단순화된 API 제공
///          자동 초기화 시퀀스 (시간 동기화 → 서비스 기능 탐색)
///          IP/Port 교체 기능으로 dual network 환경 지원
class OnvifClient
{
public:
  /// @brief OnvifClient 생성자
  /// @param io_context Boost.Asio IO context 참조
  /// @param host 카메라 호스트 주소 (IP 또는 도메인)
  /// @param port 카메라 HTTP 포트 (기본값: 80)
  /// @param username ONVIF 인증 사용자명
  /// @param password ONVIF 인증 패스워드
  OnvifClient(
    AsioContext& io_context,
    std::string host,
    int port,
    std::string username,
    std::string password);

  ~OnvifClient();

  NX_NON_COPYABLE_AND_MOVABLE(OnvifClient);

  /// @brief ONVIF 클라이언트 초기화
  /// @details 초기화 시퀀스:
  ///          1. GetSystemDateAndTime (시간 동기화)
  ///          2. GetCapabilities (서비스 URL 탐색)
  ///          3. Media/PTZ 서비스 객체 생성
  /// @return 성공 시 void, 실패 시 error_code
  nx::awaitable_expected<void> initialize();

  /// @brief 초기화 여부 확인
  /// @return 초기화 완료 시 true
  bool is_initialized() const noexcept { return m_initialized; }

  // ==================== Device Service API ====================

  /// @brief 카메라 시스템 시간 조회 (캐시됨)
  /// @return 캐시된 DateTime 정보
  std::optional<DateTime> get_system_date_time() const noexcept
  {
    return m_cached_datetime;
  }

  /// @brief 카메라 서비스 기능 정보 조회 (캐시됨)
  /// @return 캐시된 Capabilities 정보
  std::optional<OnvifCapabilities> get_capabilities() const noexcept
  {
    return m_cached_capabilities;
  }

  /// @brief 카메라 장치 정보 조회
  /// @return 장치 정보 (제조사, 모델, 펌웨어 등)
  nx::awaitable_expected<DeviceInfo> get_device_information();

  // ==================== Media Service API ====================

  /// @brief 미디어 프로필 목록 조회
  /// @return 카메라가 지원하는 모든 미디어 프로필
  nx::awaitable_expected<std::vector<MediaProfile>> get_profiles();

  /// @brief 특정 프로필의 스트림 URI 조회
  /// @param profile_token 프로필 토큰
  /// @param replace_host 반환된 URI의 호스트를 교체할 주소 (옵션)
  /// @param replace_port 반환된 URI의 포트를 교체할 포트 (옵션)
  /// @return RTSP 스트림 URI
  nx::awaitable_expected<StreamUri> get_stream_uri(
    const std::string& profile_token,
    std::optional<std::string> replace_host = std::nullopt,
    std::optional<int> replace_port = std::nullopt);

  /// @brief 메인 스트림 URI 조회 (1080p 선호, H.264 우선)
  /// @param replace_host URI 호스트 교체 (옵션)
  /// @param replace_port URI 포트 교체 (옵션)
  /// @return 메인 스트림 URI
  nx::awaitable_expected<StreamUri> get_main_stream_uri(
    std::optional<std::string> replace_host = std::nullopt,
    std::optional<int> replace_port = std::nullopt);

  /// @brief 서브 스트림 URI 조회 (720p 선호, 낮은 비트레이트)
  /// @param replace_host URI 호스트 교체 (옵션)
  /// @param replace_port URI 포트 교체 (옵션)
  /// @return 서브 스트림 URI
  nx::awaitable_expected<StreamUri> get_second_stream_uri(
    std::optional<std::string> replace_host = std::nullopt,
    std::optional<int> replace_port = std::nullopt);

  // ==================== PTZ Service API ====================

  /// @brief PTZ Continuous Move (속도 기반 이동)
  /// @param profile_token 프로필 토큰
  /// @param pan_tilt_x Pan 속도 (-1.0 ~ 1.0)
  /// @param pan_tilt_y Tilt 속도 (-1.0 ~ 1.0)
  /// @param zoom Zoom 속도 (-1.0 ~ 1.0)
  /// @param timeout_seconds 이동 타임아웃 (초), 0이면 무제한
  /// @return 성공 시 void
  nx::awaitable_expected<void> continuous_move(
    const std::string& profile_token,
    double pan_tilt_x,
    double pan_tilt_y,
    double zoom,
    int timeout_seconds = 0);

  /// @brief PTZ 이동 정지
  /// @param profile_token 프로필 토큰
  /// @param pan_tilt Pan/Tilt 정지 여부
  /// @param zoom Zoom 정지 여부
  /// @return 성공 시 void
  nx::awaitable_expected<void>
  stop(const std::string& profile_token, bool pan_tilt = true, bool zoom = true);

  /// @brief 현재 위치를 Preset으로 저장
  /// @param profile_token 프로필 토큰
  /// @param preset_name Preset 이름 (옵션)
  /// @param preset_token 기존 Preset 토큰 (수정 시, 옵션)
  /// @return 생성/수정된 Preset 토큰
  nx::awaitable_expected<std::string> set_preset(
    const std::string& profile_token,
    const std::string& preset_name = "",
    const std::string& preset_token = "");

  /// @brief 저장된 Preset 목록 조회
  /// @param profile_token 프로필 토큰
  /// @return Preset 목록
  nx::awaitable_expected<std::vector<services::PtzPreset>>
  get_presets(const std::string& profile_token);

  /// @brief Preset 위치로 이동
  /// @param profile_token 프로필 토큰
  /// @param preset_token 이동할 Preset 토큰
  /// @return 성공 시 void
  nx::awaitable_expected<void>
  goto_preset(const std::string& profile_token, const std::string& preset_token);

  /// @brief Preset 삭제
  /// @param profile_token 프로필 토큰
  /// @param preset_token 삭제할 Preset 토큰
  /// @return 성공 시 void
  nx::awaitable_expected<void>
  remove_preset(const std::string& profile_token, const std::string& preset_token);

  /// @brief Focus 연속 이동
  /// @param video_source_token 비디오 소스 토큰
  /// @param speed Focus 속도 (-1.0 ~ 1.0)
  /// @return 성공 시 void
  nx::awaitable_expected<void>
  move_focus(const std::string& video_source_token, double speed);

  /// @brief Iris 연속 이동
  /// @param video_source_token 비디오 소스 토큰
  /// @param speed Iris 속도 (-1.0 ~ 1.0)
  /// @return 성공 시 void
  nx::awaitable_expected<void>
  move_iris(const std::string& video_source_token, double speed);

  // ==================== Health Check ====================

  /// @brief 경량 접속 상태 확인 (TCP 연결 + GetSystemDateAndTime만 수행)
  nx::awaitable_expected<void> check_connection();

  // ==================== Utility ====================

  /// @brief 카메라 연결 정보 조회
  std::string get_host() const noexcept { return m_host; }
  int get_port() const noexcept { return m_port; }

private:
  // 초기화 헬퍼 메서드
  nx::awaitable_expected<void> initialize_media_service();

  nx::awaitable_expected<void> initialize_ptz_service();

  // 멤버 변수
  AsioContext& m_io_context;
  std::string m_host;
  int m_port;
  std::string m_username;
  std::string m_password;

  // SOAP 클라이언트 (모든 서비스가 공유)
  std::shared_ptr<soap::SoapClient> m_soap_client;

  // 서비스 인스턴스
  std::unique_ptr<services::DeviceService> m_device_service;
  std::unique_ptr<services::MediaService> m_media_service;
  std::unique_ptr<services::PtzService> m_ptz_service;

  // 캐시된 데이터
  std::optional<DateTime> m_cached_datetime;
  std::optional<OnvifCapabilities> m_cached_capabilities;

  // 초기화 상태
  bool m_initialized = false;
};

} // namespace nx::net::onvif
