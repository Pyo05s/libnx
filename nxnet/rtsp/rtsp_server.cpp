// 파일: rtsp_server.cpp
// 생성일: 2026-02-26
// 설명: RTSP 서버 구현

#include "rtsp_server.h"

#include <spdlog/spdlog.h>
#include <fmt/format.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>

namespace nx::net {

// ============================================================================
// 생성자/소멸자
// ============================================================================

RtspServer::RtspServer(
  AsioContext& ioc, std::shared_ptr<IRtspSessionRegistry> registry, uint16_t port)
    : m_ioc(ioc)
    , m_registry(std::move(registry))
    , m_port(port)
    , m_acceptor(ioc)
{}

RtspServer::~RtspServer()
{
  if (m_running) {
    spdlog::warn("[RtspServer] 소멸자에서 비정상 종료 감지 (port={})", m_port);
  }
}

// ============================================================================
// 서버 시작
// ============================================================================

nx::awaitable<std::error_code>
RtspServer::start()
{
  if (m_running) {
    co_return std::error_code{};
  }

  // TCP acceptor 바인딩
  boost::system::error_code ec;
  auto endpoint = boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), m_port);

  m_acceptor.open(endpoint.protocol(), ec);
  if (ec) {
    spdlog::error("[RtspServer] 소켓 열기 실패: {}", ec.message());
    co_return ec;
  }

  m_acceptor.set_option(boost::asio::socket_base::reuse_address(true), ec);
  if (ec) {
    spdlog::warn("[RtspServer] SO_REUSEADDR 설정 실패: {}", ec.message());
  }

  m_acceptor.bind(endpoint, ec);
  if (ec) {
    spdlog::error("[RtspServer] 바인딩 실패 (port={}): {}", m_port, ec.message());
    co_return ec;
  }

  m_acceptor.listen(boost::asio::socket_base::max_listen_connections, ec);
  if (ec) {
    spdlog::error("[RtspServer] listen 실패: {}", ec.message());
    co_return ec;
  }

  m_running = true;
  spdlog::info("[RtspServer] 서버 시작 (port={})", m_port);

  // Accept 루프 비동기 시작 — shared_from_this()로 수명 보장
  boost::asio::co_spawn(
    m_ioc,
    [self = shared_from_this()]() -> nx::awaitable<void> {
      co_await self->accept_loop();
    },
    boost::asio::detached);

  co_return std::error_code{};
}

// ============================================================================
// 서버 중지
// ============================================================================

nx::awaitable<void>
RtspServer::stop()
{
  if (!m_running) {
    co_return;
  }

  m_running = false;

  // Acceptor 종료
  boost::system::error_code ec;
  m_acceptor.close(ec);

  // 모든 활성 세션 종료
  std::vector<std::shared_ptr<RtspServerSession>> sessions;
  {
    std::lock_guard lock(m_mutex);
    sessions = std::move(m_sessions);
  }

  for (auto& session : sessions) {
    session->close();
  }

  spdlog::info("[RtspServer] 서버 중지 (port={})", m_port);
  co_return;
}

// ============================================================================
// Accept 루프
// ============================================================================

nx::awaitable<void>
RtspServer::accept_loop()
{
  while (m_running) {
    try {
      auto socket = co_await m_acceptor.async_accept(boost::asio::use_awaitable);

      // 종료된 세션 정리
      cleanup_sessions();

      // 새 클라이언트 세션 생성
      auto session = std::make_shared<RtspServerSession>(std::move(socket), m_registry);

      {
        std::lock_guard lock(m_mutex);
        m_sessions.push_back(session);
      }

      session->start();
    }
    catch (const boost::system::system_error& e) {
      if (e.code() == boost::asio::error::operation_aborted) {
        break; // 서버 종료
      }
      spdlog::warn("[RtspServer] Accept 오류: {}", e.what());
    }
  }
}

// ============================================================================
// 세션 정리
// ============================================================================

void
RtspServer::cleanup_sessions()
{
  std::lock_guard lock(m_mutex);
  m_sessions.erase(
    std::remove_if(
      m_sessions.begin(),
      m_sessions.end(),
      [](const auto& session) {
        // 연결이 닫힌 세션 제거
        return session.use_count() <= 1;
      }),
    m_sessions.end());
}

// ============================================================================
// 접근자
// ============================================================================

bool
RtspServer::is_running() const
{
  return m_running;
}

uint16_t
RtspServer::port() const
{
  return m_port;
}

size_t
RtspServer::client_count() const
{
  std::lock_guard lock(m_mutex);
  return m_sessions.size();
}

} // namespace nx::net
