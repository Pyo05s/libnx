// 파일: soap_client.h
// 생성일: 2026-02-17
// 설명: SOAP 통신 클라이언트 (HTTP + WS-Security)

#pragma once

#include "nxnet/auth/auth_provider.h"
#include "nxnet/http/http_client.h"
#include "nxnet/onvif/onvif_types.h"
#include "soap_types.h"
#include <nxcore/util/type_util.h>


#include <expected>
#include <memory>
#include <nxcore/util/asio_type.h>
#include <string>


namespace nx::net::onvif::soap {

// ============================================================================
// SOAP 클라이언트
// ============================================================================

/// SOAP 통신 클라이언트
/// HttpClient를 래핑하고 WS-Security 인증을 자동으로 처리
class SoapClient
{
public:
  /// 생성자
  /// @param ioc boost::asio io_context
  /// @param auth_provider 인증 제공자 (WsSecurityProvider 권장)
  SoapClient(AsioContext& ioc, std::unique_ptr<auth::AuthProvider> auth_provider);

  ~SoapClient();

  NX_NON_COPYABLE_AND_MOVABLE(SoapClient);

  // ========================================================================
  // 연결 관리
  // ========================================================================

  /// 서버 연결
  /// @param host 호스트 (IP 또는 도메인)
  /// @param port 포트 번호
  nx::awaitable<std::error_code> connect(const std::string& host, uint16_t port);

  /// 연결 종료
  nx::awaitable<std::error_code> close();

  // ========================================================================
  // SOAP 요청
  // ========================================================================

  /// SOAP 요청 전송
  /// @param service_url 서비스 URL (/onvif/device_service 등)
  /// @param action ONVIF Action (GetDeviceInformation 등)
  /// @param body SOAP Body 내용
  /// @param camera_time 카메라 시간 (WS-Security 타임스탬프용)
  /// @return SOAP 응답
  nx::awaitable_expected<SoapResponse> send_request(
    const std::string& service_url,
    const std::string& action,
    const std::string& body,
    const DateTime& camera_time);

  // ========================================================================
  // 상태 조회
  // ========================================================================

  /// 연결 상태 확인
  bool is_connected() const noexcept;

  /// 인증 제공자 갱신
  void set_auth_provider(std::unique_ptr<auth::AuthProvider> auth_provider);

private:
  // ========================================================================
  // 내부 메서드
  // ========================================================================

  /// SOAP Fault를 에러 코드로 변환
  std::error_code convert_soap_fault(const SoapResponse& response) const;

  /// 연결 확인 및 자동 재연결
  nx::awaitable<std::error_code> ensure_connected();

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  std::unique_ptr<HttpClient> m_http_client;
  std::unique_ptr<auth::AuthProvider> m_auth_provider;

  // 자동 재연결을 위한 연결 정보
  std::string m_host;
  uint16_t m_port{0};
};

} // namespace nx::net::onvif::soap
