// 파일: https_client.cpp
// 생성일: 2026-02-06
// 설명: boost::beast 기반 비동기 HTTPS 클라이언트 구현

#include "https_client.h"
#include "detail/redirect_handler.h"
#include "detail/request_helper.h"
#include <spdlog/spdlog.h>

namespace nx {
namespace net {

// ========================================================================
// 생성자 및 소멸자
// ========================================================================

HttpsClient::HttpsClient(AsioContext& ioc, uint32_t cto, uint32_t rto)
    : m_ioc(ioc)
    , m_strand(boost::asio::make_strand(ioc))
    , m_ssl_ctx(boost::asio::ssl::context::tls_client)
    , m_stream(m_strand, m_ssl_ctx)
{
  // 타임아웃 옵션 초기화
  m_options.connect_timeout = nx::milliseconds(cto);
  m_options.response_timeout = nx::milliseconds(rto);

  // SSL 컨텍스트 초기화
  init_ssl_context();

  spdlog::debug(
    "HttpsClient created (connect_timeout={}ms, response_timeout={}ms)", cto, rto);
}

HttpsClient::~HttpsClient()
{
  // 연결이 남아있으면 경고
  if (m_connected.load()) {
    spdlog::warn("HttpsClient destroyed while still connected to {}:{}", m_host, m_port);
  }

  spdlog::debug("HttpsClient destroyed");
}

// ========================================================================
// SSL 컨텍스트 초기화
// ========================================================================

void
HttpsClient::init_ssl_context()
{
  try {
    // 기본 시스템 CA 인증서 경로 설정
    m_ssl_ctx.set_default_verify_paths();

    // 인증서 검증 모드 설정
    if (m_options.ssl.verify_certificate) {
      m_ssl_ctx.set_verify_mode(boost::asio::ssl::verify_peer);

      // CA 인증서 파일이 지정된 경우 로드
      if (!m_options.ssl.ca_cert_file.empty()) {
        m_ssl_ctx.load_verify_file(m_options.ssl.ca_cert_file);
        spdlog::debug("Loaded CA certificate: {}", m_options.ssl.ca_cert_file);
      }
    }
    else {
      spdlog::warn("⚠️  SSL certificate verification is DISABLED");
      m_ssl_ctx.set_verify_mode(boost::asio::ssl::verify_none);
    }
  }
  catch (const std::exception& e) {
    spdlog::error("Failed to initialize SSL context: {}", e.what());
    throw;
  }
}

// ========================================================================
// 연결 관리
// ========================================================================

nx::awaitable<std::error_code>
HttpsClient::connect(const std::string& host, uint16_t port)
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_connect(host, port), boost::asio::use_awaitable);
}

nx::awaitable<std::error_code>
HttpsClient::do_connect(std::string host, uint16_t port)
{
  // 이미 연결되어 있으면 에러
  if (m_connected.load()) {
    spdlog::warn(
      "HttpsClient::connect() called but already connected to {}:{}", m_host, m_port);
    co_return make_error_code(HttpErrc::already_connected);
  }

  spdlog::info("HttpsClient: Connecting to {}:{} (HTTPS)...", host, port);

  try {
    // 1. DNS 해석
    boost::asio::ip::tcp::resolver resolver(m_ioc);
    auto results = co_await resolver.async_resolve(
      host, std::to_string(port), boost::asio::use_awaitable);

    // 2. connect_timeout 적용
    {
      std::lock_guard<std::mutex> lock(m_options_mutex);
      boost::beast::get_lowest_layer(m_stream).expires_after(m_options.connect_timeout);
    }

    // 3. TCP 연결
    co_await boost::beast::get_lowest_layer(m_stream).async_connect(
      results, boost::asio::use_awaitable);

    spdlog::debug("TCP connection established, starting SSL handshake...");

    // 4. SSL 핸드셰이크 (호스트명을 인자로 전달)
    auto ec = co_await do_ssl_handshake(host);
    if (ec) {
      co_return ec;
    }

    // 연결 정보 저장
    m_host = std::move(host);
    m_port = port;
    m_connected.store(true);

    spdlog::info("HttpsClient: Connected to {}:{} (HTTPS)", m_host, m_port);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    m_connected.store(false);

    // 타임아웃 에러 처리
    if (e.code() == boost::asio::error::timed_out ||
        e.code() == boost::beast::error::timeout) {
      spdlog::error("HttpsClient: Connection timeout to {}:{}", host, port);
      co_return make_error_code(HttpErrc::connect_timeout);
    }

    spdlog::error("HttpsClient: Connection failed to {}:{} - {}", host, port, e.what());
    co_return make_error_code(HttpErrc::connection_failed);
  }
}

nx::awaitable<std::error_code>
HttpsClient::do_ssl_handshake(const std::string& hostname)
{
  try {
    // SNI (Server Name Indication) 설정
    std::string server_name =
      m_options.ssl.server_name.empty() ? hostname : m_options.ssl.server_name;

    if (!SSL_set_tlsext_host_name(m_stream.native_handle(), server_name.c_str())) {
      spdlog::error("HttpsClient: Failed to set SNI hostname");
      co_return make_error_code(HttpErrc::ssl_handshake_failed);
    }

    // SSL 핸드셰이크 수행
    co_await m_stream.async_handshake(
      boost::asio::ssl::stream_base::client, boost::asio::use_awaitable);

    spdlog::debug("HttpsClient: SSL handshake completed");
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    spdlog::error(
      "HttpsClient: SSL handshake failed: {} ({})", e.what(), e.code().message());

    // SSL 관련 에러 세분화
    if (e.code().category() == boost::asio::error::get_ssl_category()) {
      co_return make_error_code(HttpErrc::ssl_verification_failed);
    }

    co_return make_error_code(HttpErrc::ssl_handshake_failed);
  }
}

nx::awaitable<std::error_code>
HttpsClient::close()
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_close(), boost::asio::use_awaitable);
}

nx::awaitable<std::error_code>
HttpsClient::do_close()
{
  if (!m_connected.load()) {
    spdlog::debug("HttpsClient::close() called but not connected");
    co_return std::error_code{};
  }

  spdlog::info("HttpsClient: Closing HTTPS connection to {}:{}...", m_host, m_port);

  try {
    // SSL shutdown
    co_await m_stream.async_shutdown(boost::asio::use_awaitable);

    // 소켓 종료
    boost::beast::error_code ec;
    boost::beast::get_lowest_layer(m_stream).socket().shutdown(
      boost::asio::ip::tcp::socket::shutdown_both, ec);

    // 연결 상태 초기화
    m_connected.store(false);
    m_host.clear();
    m_port = 443;

    spdlog::info("HTTPS connection closed");
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    m_connected.store(false);
    spdlog::warn("HttpsClient: Error while closing HTTPS connection: {}", e.what());
    co_return e.code();
  }
}

// ========================================================================
// HTTP 요청
// ========================================================================

nx::awaitable_expected<HttpResponse>
HttpsClient::send_request(const HttpRequest& request)
{
  // strand로 래핑하여 순차 실행 보장
  co_return co_await boost::asio::co_spawn(
    m_strand, do_send_request_with_redirects(request, 0), boost::asio::use_awaitable);
}

nx::awaitable_expected<HttpResponse>
HttpsClient::do_send_request(HttpRequest request)
{
  // 연결 확인
  if (!m_connected.load()) {
    spdlog::error("HttpsClient::send_request() called but not connected");
    co_return std::unexpected(make_error_code(HttpErrc::not_connected));
  }

  spdlog::debug(
    "HttpsClient: Sending {} request to {}",
    boost::beast::http::to_string(request.method), request.target);

  try {
    // response_timeout 적용
    {
      std::lock_guard<std::mutex> lock(m_options_mutex);
      boost::beast::get_lowest_layer(m_stream).expires_after(m_options.response_timeout);
    }

    // HTTP 요청 작성
    auto req =
      detail::RequestHelper::build_request(request, m_host, "nx-https-client/1.0");

    // SSL 스트림을 통해 요청 전송
    co_await boost::beast::http::async_write(m_stream, req, boost::asio::use_awaitable);

    // SSL 스트림을 통해 응답 수신
    boost::beast::http::response<boost::beast::http::string_body> res;
    co_await boost::beast::http::async_read(
      m_stream, m_buffer, res, boost::asio::use_awaitable);

    // 응답 변환
    auto response = detail::RequestHelper::convert_response(res);

    spdlog::debug("HttpsClient: Received response with status {}", response.status_code);
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

HttpsClientOptions
HttpsClient::options() const noexcept
{
  std::lock_guard<std::mutex> lock(m_options_mutex);
  return m_options;
}

void
HttpsClient::set_options(const HttpsClientOptions& options)
{
  std::lock_guard<std::mutex> lock(m_options_mutex);
  m_options = options;

  spdlog::debug(
    "HttpsClient options updated (connect_timeout={}ms, response_timeout={}ms)",
    m_options.connect_timeout.count(), m_options.response_timeout.count());

  // SSL 옵션이 변경되면 재초기화 필요 (연결 전에만 가능)
  if (!m_connected.load()) {
    init_ssl_context();
  }
}

void
HttpsClient::set_ssl_options(const SslOptions& ssl_options)
{
  std::lock_guard<std::mutex> lock(m_options_mutex);
  m_options.ssl = ssl_options;

  // SSL 옵션 변경 시 재초기화 (연결 전에만 가능)
  if (!m_connected.load()) {
    init_ssl_context();
  }
  else {
    spdlog::warn("HttpsClient: Cannot change SSL options while connected");
  }
}

// ========================================================================
// 리다이렉션 처리
// ========================================================================

nx::awaitable_expected<HttpResponse>
HttpsClient::do_send_request_with_redirects(HttpRequest request, uint32_t redirect_count)
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
      spdlog::error("HttpsClient: Too many redirects ({})", redirect_count);
      co_return std::unexpected(make_error_code(HttpErrc::too_many_redirects));
    }
  }

  // Location 헤더 확인
  auto location = detail::RedirectHandler::find_location_header(response.headers);
  if (location.empty()) {
    spdlog::error("HttpsClient: Redirect response without Location header");
    co_return std::unexpected(make_error_code(HttpErrc::invalid_redirect));
  }

  spdlog::info(
    "HttpsClient: Following redirect ({}/max) to: {}", redirect_count + 1, location);

  // Location 파싱
  auto parse_result = detail::RedirectHandler::parse_location(
    location, m_host, m_port, request.target,
    true); // HTTPS는 true
  if (!parse_result.has_value()) {
    co_return std::unexpected(parse_result.error());
  }

  auto& redirect_info = parse_result.value();

  // HTTPS→HTTP 다운그레이드 체크
  if (!redirect_info.is_https) {
    std::lock_guard<std::mutex> lock(m_options_mutex);
    if (!m_options.allow_downgrade) {
      spdlog::error("HttpsClient: HTTPS to HTTP downgrade is not allowed");
      co_return std::unexpected(make_error_code(HttpErrc::downgrade_forbidden));
    }

    spdlog::warn("HttpsClient: ⚠️  Downgrading from HTTPS to HTTP");

    // 민감한 헤더 제거
    HttpRequest new_request = request;
    detail::RedirectHandler::strip_sensitive_headers(new_request.headers);

    // HTTP 클라이언트로 전환해야 하지만, 현재 구조에서는 불가능
    // 따라서 에러 반환
    spdlog::error("HttpsClient: Cannot downgrade to HTTP within HttpsClient");
    co_return std::unexpected(make_error_code(HttpErrc::downgrade_forbidden));
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
