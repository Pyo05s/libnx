// 파일: https_client.h
// 생성일: 2026-02-06
// 설명: boost::beast 기반 비동기 HTTPS 클라이언트

#pragma once

#include "http_error.h"
#include "http_client.h"
#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <nxcore/util/asio_type.h>
#include <boost/asio/ssl.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>

#include <expected>
#include <system_error>
#include <map>
#include <string>
#include <atomic>
#include <mutex>

namespace nx {
namespace net {

// SSL/TLS 옵션
struct SslOptions
{
  bool verify_certificate{true}; // 인증서 검증 (기본 활성화)
  std::string ca_cert_file;      // CA 인증서 파일 (선택)
  std::string server_name;       // SNI 서버 이름 (선택, 기본은 host 사용)
};

// HTTPS 클라이언트 옵션
struct HttpsClientOptions
{
  nx::milliseconds connect_timeout{5000};   // 연결 타임아웃 (기본 5초)
  nx::milliseconds response_timeout{30000}; // 응답 타임아웃 (기본 30초)
  bool follow_redirects{true};              // 리다이렉션 자동 처리 (기본 활성화)
  uint32_t max_redirects{5};                // 최대 리다이렉션 횟수 (기본 5회)
  bool allow_downgrade{false};              // HTTPS→HTTP 다운그레이드 허용 (기본 금지)
  SslOptions ssl;                           // SSL 설정
};

// HTTPS 클라이언트 클래스
class HttpsClient
{
public:
  // 외부에서 io_context 주입
  // cto: connect timeout (ms), rto: response timeout (ms)
  explicit HttpsClient(AsioContext& ioc, uint32_t cto = 5000, uint32_t rto = 30000);

  ~HttpsClient();

  NX_NON_COPYABLE_AND_MOVABLE(HttpsClient);

  // ========================================================================
  // 연결 관리 (코루틴)
  // ========================================================================

  // HTTPS 서버 연결 (기본 포트 443)
  nx::awaitable<std::error_code> connect(const std::string& host, uint16_t port = 443);

  // 연결 종료
  nx::awaitable<std::error_code> close();

  // ========================================================================
  // HTTP 요청 (코루틴)
  // ========================================================================

  // 범용 HTTPS 요청 메서드 (response_timeout 적용)
  // HttpRequest 재사용
  nx::awaitable_expected<HttpResponse> send_request(const HttpRequest& request);

  // ========================================================================
  // 상태 조회
  // ========================================================================

  // 연결 상태 확인 (스레드 안전)
  bool is_connected() const noexcept { return m_connected.load(); }

  // 현재 옵션 조회
  HttpsClientOptions options() const noexcept;

  // 옵션 변경 (스레드 안전)
  void set_options(const HttpsClientOptions& options);

  // SSL 옵션 변경
  void set_ssl_options(const SslOptions& ssl_options);

private:
  // ========================================================================
  // 내부 구현 메서드
  // ========================================================================

  // 실제 연결 수행 (strand 내에서 실행)
  nx::awaitable<std::error_code> do_connect(std::string host, uint16_t port);

  // SSL 핸드셰이크 수행
  nx::awaitable<std::error_code> do_ssl_handshake(const std::string& hostname);

  // 실제 요청 전송 (strand 내에서 실행)
  nx::awaitable_expected<HttpResponse> do_send_request(HttpRequest request);

  // 리다이렉션 처리를 포함한 요청 전송
  nx::awaitable_expected<HttpResponse>
  do_send_request_with_redirects(HttpRequest request, uint32_t redirect_count);

  // 실제 연결 종료 (strand 내에서 실행)
  nx::awaitable<std::error_code> do_close();

  // SSL 컨텍스트 초기화
  void init_ssl_context();

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  AsioContext& m_ioc;

  // 멀티스레드 안전성을 위한 strand
  AsioStrand m_strand;

  // SSL 컨텍스트
  boost::asio::ssl::context m_ssl_ctx;

  // Beast SSL 스트림 (TCP + SSL)
  boost::beast::ssl_stream<boost::beast::tcp_stream> m_stream;

  // 읽기 버퍼
  boost::beast::flat_buffer m_buffer;

  // 연결 정보
  std::string m_host;
  uint16_t m_port{443};
  std::atomic<bool> m_connected{false};

  // 옵션 (타임아웃 및 SSL 설정)
  HttpsClientOptions m_options;
  mutable std::mutex m_options_mutex; // options 접근 보호
};

} // namespace net
} // namespace nx
