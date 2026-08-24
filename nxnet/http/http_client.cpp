// 파일: http_client.cpp
// 생성일: 2026-02-06
// 설명: boost::beast 기반 비동기 HTTP 클라이언트 구현

#include "http_client.h"
#include "detail/redirect_handler.h"
#include "detail/request_helper.h"
#include <spdlog/spdlog.h>

namespace nx {
namespace net {

// ========================================================================
// 생성자 및 소멸자
// ========================================================================

HttpClient::HttpClient(AsioContext& ioc, nx::milliseconds cto, nx::milliseconds rto)
    : m_ioc(ioc)
    , m_strand(boost::asio::make_strand(ioc))
    , m_stream(m_strand)
{
  // 타임아웃 옵션 초기화
  m_options.connect_timeout = cto;
  m_options.response_timeout = rto;

  spdlog::debug(
    "HttpClient created (connect_timeout={}ms, response_timeout={}ms)", cto.count(),
    rto.count());
}

HttpClient::~HttpClient()
{
  // 연결이 남아있으면 경고
  if (m_connected.load()) {
    spdlog::warn("HttpClient destroyed while still connected to {}:{}", m_host, m_port);
  }

  spdlog::debug("HttpClient destroyed");
}

// ========================================================================
// 연결 관리
// ========================================================================

nx::awaitable<std::error_code>
HttpClient::connect(const std::string& host, uint16_t port)
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_connect(host, port), boost::asio::use_awaitable);
}

nx::awaitable<std::error_code>
HttpClient::do_connect(std::string host, uint16_t port)
{
  // 이미 연결되어 있으면 에러
  if (m_connected.load()) {
    spdlog::warn(
      "HttpClient::connect() called but already connected to {}:{}", m_host, m_port);
    co_return make_error_code(HttpErrc::already_connected);
  }

  spdlog::info("HttpClient: Connecting to {}:{}...", host, port);

  try {
    // 이전 연결 잔여 데이터 정리
    m_buffer.consume(m_buffer.size());

    // DNS 해석
    boost::asio::ip::tcp::resolver resolver(m_ioc);
    auto results = co_await resolver.async_resolve(
      host, std::to_string(port), boost::asio::use_awaitable);

    // connect_timeout 적용
    {
      std::lock_guard<std::mutex> lock(m_options_mutex);
      m_stream.expires_after(m_options.connect_timeout);
    }

    // 연결
    co_await m_stream.async_connect(results, boost::asio::use_awaitable);

    // 연결 정보 저장
    m_host = std::move(host);
    m_port = port;
    m_connected.store(true);

    spdlog::info("HttpClient: Connected to {}:{}", m_host, m_port);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    m_connected.store(false);

    // 타임아웃 에러 처리
    if (e.code() == boost::asio::error::timed_out ||
        e.code() == boost::beast::error::timeout) {
      spdlog::error("HttpClient: Connection timeout to {}:{}", host, port);
      co_return make_error_code(HttpErrc::connect_timeout);
    }

    spdlog::error("HttpClient: Connection failed to {}:{} - {}", host, port, e.what());
    co_return make_error_code(HttpErrc::connection_failed);
  }
}

nx::awaitable<std::error_code>
HttpClient::close()
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_close(), boost::asio::use_awaitable);
}

nx::awaitable<std::error_code>
HttpClient::do_close()
{
  if (!m_connected.load()) {
    spdlog::debug("HttpClient::close() called but not connected");
    co_return std::error_code{};
  }

  spdlog::info("Closing connection to {}:{}...", m_host, m_port);

  try {
    // 소켓 종료
    boost::beast::error_code ec;
    m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);

    // 연결 상태 초기화
    m_connected.store(false);
    m_host.clear();
    m_port = 0;

    spdlog::info("Connection closed");
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    m_connected.store(false);
    spdlog::error("Error while closing connection: {}", e.what());
    co_return e.code();
  }
}

// ========================================================================
// HTTP 요청
// ========================================================================

nx::awaitable_expected<HttpResponse>
HttpClient::send_request(const HttpRequest& request)
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_send_request_with_redirects(request, 0), boost::asio::use_awaitable);
}

nx::awaitable_expected<HttpResponse>
HttpClient::do_send_request(HttpRequest request)
{
  // 연결 확인
  if (!m_connected.load()) {
    spdlog::error("HttpClient::send_request() called but not connected");
    co_return std::unexpected(make_error_code(HttpErrc::not_connected));
  }

  spdlog::debug(
    "HttpClient: Sending {} request to {}", boost::beast::http::to_string(request.method),
    request.target);

  try {
    // response_timeout 적용
    {
      std::lock_guard<std::mutex> lock(m_options_mutex);
      m_stream.expires_after(m_options.response_timeout);
    }

    // HTTP 요청 작성
    auto req =
      detail::RequestHelper::build_request(request, m_host, "nx-http-client/1.0");

    // 요청 전송
    co_await boost::beast::http::async_write(m_stream, req, boost::asio::use_awaitable);

    // 응답 수신
    boost::beast::http::response<boost::beast::http::string_body> res;
    co_await boost::beast::http::async_read(
      m_stream, m_buffer, res, boost::asio::use_awaitable);

    // 응답 변환
    auto response = detail::RequestHelper::convert_response(res);

    spdlog::debug("HttpClient: Received response with status {}", response.status_code);

    // Connection: close 처리 - 서버가 연결을 닫을 예정이면 클라이언트도 정리
    if (res.need_eof()) {
      spdlog::debug("HttpClient: Server requested connection close");
      boost::beast::error_code bec;
      m_stream.socket().shutdown(boost::asio::ip::tcp::socket::shutdown_both, bec);
      m_connected.store(false);
    }

    co_return response;
  }
  catch (const boost::system::system_error& e) {
    // 에러 처리
    auto ec = detail::RequestHelper::handle_request_error(e, m_connected, request.target);
    co_return std::unexpected(ec);
  }
}

// ========================================================================
// 옵션 관리
// ========================================================================

HttpClientOptions
HttpClient::options() const noexcept
{
  std::lock_guard<std::mutex> lock(m_options_mutex);
  return m_options;
}

void
HttpClient::set_options(const HttpClientOptions& options)
{
  std::lock_guard<std::mutex> lock(m_options_mutex);
  m_options = options;

  spdlog::debug(
    "HttpClient options updated (connect_timeout={}ms, response_timeout={}ms)",
    m_options.connect_timeout.count(), m_options.response_timeout.count());
}

// ========================================================================
// 리다이렉션 처리
// ========================================================================

nx::awaitable_expected<HttpResponse>
HttpClient::do_send_request_with_redirects(HttpRequest request, uint32_t redirect_count)
{
  // 기본 요청 전송
  auto result = co_await do_send_request(request);
  if (!result.has_value()) {
    co_return std::unexpected(result.error());
  }

  auto& response = result.value();

  // 리다이렉션 체크 (301, 302, 303, 307, 308)
  if (!detail::RedirectHandler::is_redirect_status(response.status_code)) {
    co_return response;
  }

  // 리다이렉션이 비활성화되어 있으면 응답 반환
  {
    std::lock_guard<std::mutex> lock(m_options_mutex);
    if (!m_options.follow_redirects) {
      co_return response;
    }
  }

  // 최대 리다이렉션 횟수 체크
  {
    std::lock_guard<std::mutex> lock(m_options_mutex);
    if (redirect_count >= m_options.max_redirects) {
      spdlog::error("HttpClient: Too many redirects ({})", redirect_count);
      co_return std::unexpected(make_error_code(HttpErrc::too_many_redirects));
    }
  }

  // Location 헤더 확인
  auto location = detail::RedirectHandler::find_location_header(response.headers);
  if (location.empty()) {
    spdlog::error("HttpClient: Redirect response without Location header");
    co_return std::unexpected(make_error_code(HttpErrc::invalid_redirect));
  }

  spdlog::info(
    "HttpClient: Following redirect ({}/max) to: {}", redirect_count + 1, location);

  // Location 파싱
  auto parse_result = detail::RedirectHandler::parse_location(
    location, m_host, m_port, request.target,
    false); // HTTP는 false
  if (!parse_result.has_value()) {
    co_return std::unexpected(parse_result.error());
  }

  auto& redirect_info = parse_result.value();

  // HTTPS 업그레이드 차단 (HTTP 클라이언트는 HTTPS로 전환 불가)
  if (redirect_info.is_https) {
    spdlog::error("HttpClient: Cannot upgrade HTTP to HTTPS");
    co_return std::unexpected(make_error_code(HttpErrc::https_required));
  }

  // 다른 호스트로 리다이렉션이면 재연결 필요
  if (redirect_info.host != m_host || redirect_info.port != m_port) {
    // 기존 연결 종료
    co_await close();

    // 새 호스트로 연결
    auto ec = co_await connect(redirect_info.host, redirect_info.port);
    if (ec) {
      co_return std::unexpected(ec);
    }
  }

  // POST→GET 변환 필요 여부 체크
  HttpRequest new_request = request;
  new_request.target = redirect_info.target;
  if (detail::RedirectHandler::should_convert_to_get(
        response.status_code, request.method)) {
    new_request.method = boost::beast::http::verb::get;
    new_request.body.clear();
  }

  // 재귀 호출로 리다이렉션 처리
  co_return co_await do_send_request_with_redirects(new_request, redirect_count + 1);
}

} // namespace net
} // namespace nx
