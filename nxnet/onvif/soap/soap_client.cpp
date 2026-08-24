// 파일: soap_client.cpp
// 생성일: 2026-02-17
// 설명: SOAP 통신 클라이언트 구현

#include "soap_client.h"
#include "nxnet/http/http_types.h"
#include "nxnet/onvif/onvif_error.h"
#include "soap_envelope.h"
#include <nxcore/util/debug_util.h>
#include <spdlog/spdlog.h>

namespace nx::net::onvif::soap {

// ============================================================================
// 생성자/소멸자
// ============================================================================

SoapClient::SoapClient(
  AsioContext& ioc, std::unique_ptr<auth::AuthProvider> auth_provider)
    : m_http_client(std::make_unique<HttpClient>(ioc))
    , m_auth_provider(std::move(auth_provider))
{
  NX_ASSERT(m_auth_provider != nullptr);
}

SoapClient::~SoapClient() = default;

// ============================================================================
// 연결 관리
// ============================================================================

nx::awaitable<std::error_code>
SoapClient::connect(const std::string& host, uint16_t port)
{
  m_host = host;
  m_port = port;
  return m_http_client->connect(host, port);
}

nx::awaitable<std::error_code>
SoapClient::close()
{
  return m_http_client->close();
}

// ============================================================================
// SOAP 요청
// ============================================================================

nx::awaitable_expected<SoapResponse>
SoapClient::send_request(
  const std::string& service_url,
  const std::string& action,
  const std::string& body,
  const DateTime& camera_time)
{
  // 연결 확인 및 자동 재연결 (Connection: close 대응)
  auto reconnect_ec = co_await ensure_connected();
  if (reconnect_ec) {
    co_return std::unexpected(reconnect_ec);
  }

  // 1. WS-Security 헤더 생성
  std::optional<std::string> security_header;
  if (m_auth_provider) {
    auto sec_result = m_auth_provider->generate_soap_security_header();
    if (!sec_result.has_value()) {
      co_return std::unexpected(sec_result.error());
    }
    security_header = sec_result.value();
  }

  // 2. SOAP 요청 메시지 생성
  SoapRequest soap_request;
  soap_request.action = action;
  soap_request.body = body;
  soap_request.security_header = security_header;

  // 3. SOAP Envelope 생성
  auto envelope_result = create_soap_envelope(soap_request, camera_time);
  if (!envelope_result.has_value()) {
    co_return std::unexpected(envelope_result.error());
  }

  std::string soap_xml = envelope_result.value();

  // 4. HTTP 요청 생성
  HttpRequest http_request;
  http_request.method = boost::beast::http::verb::post;
  http_request.target = service_url;
  http_request.body = soap_xml;
  http_request.headers.set(
    boost::beast::http::field::content_type, "application/soap+xml; charset=utf-8");
  http_request.headers.set("SOAPAction", action);

  // 5. HTTP 요청 전송
  auto http_response = co_await m_http_client->send_request(http_request);
  if (!http_response.has_value()) {
    co_return std::unexpected(http_response.error());
  }

  // 6. HTTP 상태 코드 확인
  unsigned int status_code = http_response->status_code;
  if (status_code == 401) {
    co_return std::unexpected(make_error_code(OnvifError::kNotAuthorized));
  }

  // HTTP 4xx/5xx 에러 시 SOAP Fault 파싱 시도
  if (status_code >= 400) {
    auto fault_response = parse_soap_response(http_response->body);
    if (fault_response.has_value() && fault_response->is_fault) {
      std::error_code fault_ec = convert_soap_fault(*fault_response);
      spdlog::warn(
        "SOAP Fault (HTTP {}): {} - {}", status_code, fault_response->fault_code,
        fault_response->fault_reason);
      co_return std::unexpected(fault_ec);
    }
    co_return std::unexpected(make_error_code(OnvifError::kNetworkError));
  }

  // 7. SOAP 응답 파싱
  auto soap_response = parse_soap_response(http_response->body);
  if (!soap_response.has_value()) {
    co_return std::unexpected(soap_response.error());
  }

  // 8. SOAP Fault 처리
  if (soap_response->is_fault) {
    std::error_code fault_ec = convert_soap_fault(*soap_response);
    co_return std::unexpected(fault_ec);
  }

  co_return soap_response.value();
}

// ============================================================================
// 상태 조회
// ============================================================================

bool
SoapClient::is_connected() const noexcept
{
  return m_http_client->is_connected();
}

void
SoapClient::set_auth_provider(std::unique_ptr<auth::AuthProvider> auth_provider)
{
  m_auth_provider = std::move(auth_provider);
}

// ============================================================================
// 내부 메서드
// ============================================================================

std::error_code
SoapClient::convert_soap_fault(const SoapResponse& response) const
{
  // SOAP Fault Subcode에서 ONVIF 에러 매핑
  if (response.fault_subcode == fault::kNotAuthorized) {
    return make_error_code(OnvifError::kNotAuthorized);
  }
  else if (response.fault_subcode == fault::kActionNotSupported) {
    return make_error_code(OnvifError::kActionNotSupported);
  }
  else if (response.fault_subcode == fault::kInvalidArgVal ||
           response.fault_subcode == fault::kInvalidArgs) {
    return make_error_code(OnvifError::kInvalidArguments);
  }
  else if (response.fault_subcode == fault::kOperationProhibited) {
    return make_error_code(OnvifError::kOperationProhibited);
  }

  // Fault Code 확인
  if (response.fault_code == fault::kVersionMismatch) {
    return make_error_code(OnvifError::kSoapVersionMismatch);
  }
  else if (response.fault_code == fault::kMustUnderstand) {
    return make_error_code(OnvifError::kSoapMustUnderstand);
  }

  // 기본 SOAP Fault
  return make_error_code(OnvifError::kSoapFault);
}

nx::awaitable<std::error_code>
SoapClient::ensure_connected()
{
  if (m_http_client->is_connected()) {
    co_return std::error_code{};
  }

  // 연결 정보가 없으면 재연결 불가
  if (m_host.empty()) {
    co_return make_error_code(OnvifError::kNetworkError);
  }

  spdlog::debug("SoapClient auto-reconnecting to {}:{}", m_host, m_port);
  co_return co_await m_http_client->connect(m_host, m_port);
}

} // namespace nx::net::onvif::soap
