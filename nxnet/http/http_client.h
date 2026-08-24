// 파일: http_client.h
// 생성일: 2026-02-06
// 설명: boost::beast 기반 비동기 HTTP 클라이언트

#pragma once

#include "http_error.h"
#include "http_types.h"

#include <nxcore/util/time_util.h>
#include <nxcore/util/type_util.h>

#include <boost/beast.hpp>
#include <nxcore/util/asio_type.h>

#include <expected>
#include <system_error>

#include <atomic>
#include <map>
#include <mutex>
#include <string>

namespace nx {
namespace net {

// HTTP 클라이언트 클래스
class HttpClient
{
public:
  // 외부에서 io_context 주입
  // cto: 연결 타임아웃, rto: 응답 타임아웃
  explicit HttpClient(
    AsioContext& ioc,
    nx::milliseconds cto = nx::milliseconds{5000},
    nx::milliseconds rto = nx::milliseconds{30000});

  ~HttpClient();

  NX_NON_COPYABLE_AND_MOVABLE(HttpClient);

  // ========================================================================
  // 연결 관리 (코루틴)
  // ========================================================================

  // 서버 연결 (connect_timeout 적용)
  nx::awaitable<std::error_code> connect(const std::string& host, uint16_t port);

  // 연결 종료
  nx::awaitable<std::error_code> close();

  // ========================================================================
  // HTTP 요청 (코루틴)
  // ========================================================================

  // 범용 HTTP 요청 메서드 (response_timeout 적용)
  // 사용자는 HttpRequest에 method, target, body, headers를 설정하여 호출
  nx::awaitable_expected<HttpResponse> send_request(const HttpRequest& request);

  // ========================================================================
  // 상태 조회
  // ========================================================================

  // 연결 상태 확인 (스레드 안전)
  bool is_connected() const noexcept { return m_connected.load(); }

  // 현재 옵션 조회
  HttpClientOptions options() const noexcept;

  // 옵션 변경 (스레드 안전)
  void set_options(const HttpClientOptions& options);

private:
  // ========================================================================
  // 내부 구현 메서드
  // ========================================================================

  // 실제 연결 수행 (strand 내에서 실행)
  nx::awaitable<std::error_code> do_connect(std::string host, uint16_t port);

  // 실제 요청 전송 (strand 내에서 실행)
  nx::awaitable_expected<HttpResponse> do_send_request(HttpRequest request);

  // 리다이렉션 처리를 포함한 요청 전송
  nx::awaitable_expected<HttpResponse>
  do_send_request_with_redirects(HttpRequest request, uint32_t redirect_count);

  // 실제 연결 종료 (strand 내에서 실행)
  nx::awaitable<std::error_code> do_close();

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  AsioContext& m_ioc;

  // 멀티스레드 안전성을 위한 strand
  AsioStrand m_strand;

  // Beast TCP 스트림
  boost::beast::tcp_stream m_stream;

  // 읽기 버퍼
  boost::beast::flat_buffer m_buffer;

  // 연결 정보
  std::string m_host;
  uint16_t m_port{0};
  std::atomic<bool> m_connected{false};

  // 옵션 (타임아웃 설정 등)
  HttpClientOptions m_options;
  mutable std::mutex m_options_mutex; // options 접근 보호
};

} // namespace net
} // namespace nx
