// 파일: device_service.h
// 생성일: 2026-02-19
// 설명: ONVIF Device Management Service

#pragma once

#include "nxnet/onvif/soap/soap_client.h"
#include "nxnet/onvif/onvif_types.h"
#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <expected>
#include <memory>
#include <string>

namespace nx::net::onvif::services {

// ============================================================================
// Device Service
// ============================================================================

/// ONVIF Device Management Service (tds)
/// Device 정보 조회, 시간 동기화, Capabilities 획득 등을 담당
class DeviceService
{
public:
  /// 생성자
  /// @param soap_client SOAP 클라이언트 (공유)
  /// @param service_url Device Service URL (/onvif/device_service 등)
  explicit DeviceService(
    std::shared_ptr<soap::SoapClient> soap_client, std::string service_url);

  ~DeviceService();

  NX_NON_COPYABLE_AND_MOVABLE(DeviceService);

  // ========================================================================
  // ONVIF Device Service 메서드
  // ========================================================================

  /// GetSystemDateAndTime
  /// 카메라 시스템 시간 조회 (인증 불필요)
  /// @return 카메라 DateTime
  nx::awaitable_expected<DateTime> get_system_date_and_time();

  /// GetCapabilities
  /// ONVIF 기능 및 서비스 주소 조회
  /// @return ONVIF Capabilities
  nx::awaitable_expected<OnvifCapabilities> get_capabilities();

  /// GetDeviceInformation
  /// 장비 정보 조회 (제조사, 모델, 펌웨어 등)
  /// @return 장비 정보
  nx::awaitable_expected<DeviceInfo> get_device_information();

  /// GetServices
  /// 지원되는 모든 서비스 목록 조회
  /// @param include_capability 각 서비스의 기능 포함 여부
  /// @return 서비스 목록 (이름, 네임스페이스, URL 등)
  nx::awaitable_expected<std::vector<std::string>>
  get_services(bool include_capability = false);

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

  /// GetSystemDateAndTime 응답 파싱
  nx::expected<DateTime>
  parse_system_date_and_time_response(const std::string& response_body);

  /// GetCapabilities 응답 파싱
  nx::expected<OnvifCapabilities>
  parse_capabilities_response(const std::string& response_body);

  /// GetDeviceInformation 응답 파싱
  nx::expected<DeviceInfo>
  parse_device_information_response(const std::string& response_body);

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  std::shared_ptr<soap::SoapClient> m_soap_client;
  std::string m_service_url;
};

} // namespace nx::net::onvif::services
