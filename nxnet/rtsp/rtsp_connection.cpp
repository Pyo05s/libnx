// 파일: rtsp_connection.cpp
// 생성일: 2026-02-23
// 설명: RTSP 연결 관리 구현

#include "rtsp_connection.h"
#include "rtsp_parser.h"

#include <spdlog/spdlog.h>
#include <boost/asio/experimental/awaitable_operators.hpp>

#include <cstring>

namespace nx::net {

using namespace boost::asio::experimental::awaitable_operators;

namespace {
// 컴팩션 트리거 임계값: 리드 오프셋이 이 값을 넘으면 불필요한 선두 공간을 정리
static constexpr size_t kCompactThreshold = 16384;
} // namespace

RtspConnection::RtspConnection(
  AsioContext& ioc, nx::milliseconds connect_timeout, nx::milliseconds response_timeout)
    : m_ioc(ioc)
    , m_socket(ioc)
    , m_connect_timeout(connect_timeout)
    , m_response_timeout(response_timeout)
{
  m_recv_buffer.reserve(32768); // 32KB 예약 — 스트리밍 중 재할당 방지
}

RtspConnection::~RtspConnection()
{
  boost::system::error_code ec;
  if (m_socket.is_open()) {
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
  }
}

nx::awaitable<std::error_code>
RtspConnection::connect(const std::string& host, uint16_t port)
{
  if (m_connected) {
    co_return make_error_code(RtspErrc::already_connected);
  }

  try {
    boost::asio::ip::tcp::resolver resolver(m_ioc);
    auto endpoints = co_await resolver.async_resolve(
      host,
      std::to_string(port),
      boost::asio::use_awaitable);

    // 타임아웃 타이머
    AsioSteadyTimer timer(m_ioc);
    timer.expires_after(m_connect_timeout);

    // 연결 시도 (타임아웃 적용)
    auto result = co_await (
      boost::asio::async_connect(m_socket, endpoints, boost::asio::use_awaitable)
      || timer.async_wait(boost::asio::use_awaitable));

    // variant index 0: 연결 성공, 1: 타임아웃
    if (result.index() == 1) {
      // 타임아웃
      boost::system::error_code ec;
      m_socket.close(ec);
      co_return make_error_code(RtspErrc::connect_timeout);
    }

    // TCP_NODELAY 설정
    boost::system::error_code ec;
    m_socket.set_option(boost::asio::ip::tcp::no_delay(true), ec);

    m_connected = true;
    spdlog::debug("RTSP 연결 성공: {}:{}", host, port);

    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    spdlog::error("RTSP 연결 실패: {}", e.what());
    co_return make_error_code(RtspErrc::connection_failed);
  }
}

nx::awaitable<std::error_code>
RtspConnection::send(const std::string& data)
{
  if (!m_connected) {
    co_return make_error_code(RtspErrc::not_connected);
  }

  try {
    co_await boost::asio::async_write(
      m_socket,
      boost::asio::buffer(data),
      boost::asio::use_awaitable);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    spdlog::error("RTSP 전송 실패: {}", e.what());
    m_connected = false;
    co_return make_error_code(RtspErrc::send_failed);
  }
}

nx::awaitable_expected<RtspResponse>
RtspConnection::receive_response()
{
  if (!m_connected) {
    co_return std::unexpected(make_error_code(RtspErrc::not_connected));
  }

  // 타임아웃 타이머
  AsioSteadyTimer timer(m_ioc);
  timer.expires_after(m_response_timeout);

  try {
    while (true) {
      // 버퍼에 인터리브드 데이터($)가 있으면 건너뜀
      while (available() > 0 && m_recv_buffer[m_recv_offset] == '$') {
        // $ + channel(1) + length(2) = 4 바이트 헤더
        if (available() < 4) {
          auto ec = co_await read_with_timeout(timer);
          if (ec) {
            co_return std::unexpected(ec);
          }
          continue;
        }

        uint16_t frame_len = static_cast<uint16_t>(
          (m_recv_buffer[m_recv_offset + 2] << 8) | m_recv_buffer[m_recv_offset + 3]);

        size_t total_frame = 4 + frame_len;
        if (available() < total_frame) {
          auto ec = co_await read_with_timeout(timer);
          if (ec) {
            co_return std::unexpected(ec);
          }
          continue;
        }

        // 인터리브드 프레임은 무시 (응답 대기 중)
        m_recv_offset += total_frame;
      }

      // RTSP 응답 완전성 확인
      auto sv = std::string_view{
        reinterpret_cast<const char*>(m_recv_buffer.data() + m_recv_offset),
        available()};
      auto response_end = RtspParser::find_response_end(sv);
      if (response_end > 0) {
        auto result = RtspParser::parse_response(sv.substr(0, response_end));
        m_recv_offset += response_end;
        co_return result;
      }

      // 타임아웃 적용 데이터 수신
      auto ec = co_await read_with_timeout(timer);
      if (ec) {
        co_return std::unexpected(ec);
      }
    }
  }
  catch (const boost::system::system_error& e) {
    spdlog::error("RTSP 응답 수신 실패: {}", e.what());
    co_return std::unexpected(make_error_code(RtspErrc::receive_failed));
  }
}

nx::awaitable_expected<RtspConnection::InterleavedFrame>
RtspConnection::receive_interleaved_frame()
{
  if (!m_connected) {
    co_return std::unexpected(make_error_code(RtspErrc::not_connected));
  }

  // 이전 프레임 소비 확정: caller가 span을 다 쓴 후 다시 호출하므로 안전
  m_recv_offset += m_pending_consume;
  m_pending_consume = 0;
  if (m_recv_offset > kCompactThreshold) {
    compact();
  }

  try {
    while (true) {
      // RTSP 응답이 먼저 오면 건너뜨 (스트리밍 중 대대 RTCP/RTSP 메시지 처리)
      while (available() > 0 && m_recv_buffer[m_recv_offset] != '$') {
        auto sv = std::string_view{
          reinterpret_cast<const char*>(m_recv_buffer.data() + m_recv_offset),
          available()};
        auto response_end = RtspParser::find_response_end(sv);
        if (response_end > 0) {
          m_recv_offset += response_end;
        }
        else {
          auto ec = co_await read_more_data();
          if (ec) {
            co_return std::unexpected(ec);
          }
        }
      }

      // $ 프레임 확인
      if (available() >= 4 && m_recv_buffer[m_recv_offset] == '$') {
        uint8_t channel = m_recv_buffer[m_recv_offset + 1];
        uint16_t frame_len = static_cast<uint16_t>(
          (m_recv_buffer[m_recv_offset + 2] << 8) | m_recv_buffer[m_recv_offset + 3]);

        size_t total_frame = 4 + frame_len;
        if (available() >= total_frame) {
          // 복사 없이 버퍼 직접 참조 — 다음 호출 시 m_pending_consume 만큼
          // 소비
          InterleavedFrame frame;
          frame.channel = channel;
          frame.data = std::span<const uint8_t>(
            m_recv_buffer.data() + m_recv_offset + 4,
            frame_len);
          m_pending_consume = total_frame;
          co_return frame;
        }
      }

      auto ec = co_await read_more_data();
      if (ec) {
        co_return std::unexpected(ec);
      }
    }
  }
  catch (const boost::system::system_error&) {
    co_return std::unexpected(make_error_code(RtspErrc::receive_failed));
  }
}

nx::awaitable<void>
RtspConnection::close()
{
  if (m_socket.is_open()) {
    boost::system::error_code ec;
    m_socket.shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket.close(ec);
  }
  m_connected = false;
  m_recv_buffer.clear();
  m_recv_offset = 0;
  m_pending_consume = 0;
  co_return;
}

void
RtspConnection::compact()
{
  if (m_recv_offset == 0) {
    return;
  }
  // 남은 데이터를 버퍼 선두로 이동 (memmove 1회)
  const size_t remaining = m_recv_buffer.size() - m_recv_offset;
  if (remaining > 0) {
    std::memmove(m_recv_buffer.data(), m_recv_buffer.data() + m_recv_offset, remaining);
  }
  m_recv_buffer.resize(remaining);
  m_recv_offset = 0;
}

bool
RtspConnection::is_connected() const noexcept
{
  return m_connected && m_socket.is_open();
}

nx::awaitable<std::error_code>
RtspConnection::read_more_data()
{
  try {
    std::array<char, 8192> buffer;
    auto bytes = co_await m_socket.async_read_some(
      boost::asio::buffer(buffer),
      boost::asio::use_awaitable);

    const auto* src = reinterpret_cast<const uint8_t*>(buffer.data());
    m_recv_buffer.insert(m_recv_buffer.end(), src, src + bytes);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    if (e.code() == boost::asio::error::eof) {
      m_connected = false;
      co_return make_error_code(RtspErrc::connection_closed);
    }
    if (e.code() == boost::asio::error::operation_aborted) {
      // cancel() 호출에 의한 취소 — 연결 상태는 유지
      co_return make_error_code(RtspErrc::receive_failed);
    }
    m_connected = false;
    co_return make_error_code(RtspErrc::receive_failed);
  }
}

nx::awaitable<std::error_code>
RtspConnection::read_with_timeout(AsioSteadyTimer& timer)
{
  try {
    std::array<char, 8192> buffer;

    auto result = co_await (
      m_socket.async_read_some(boost::asio::buffer(buffer), boost::asio::use_awaitable)
      || timer.async_wait(boost::asio::use_awaitable));

    if (result.index() == 1) {
      // 타임아웃 발생
      boost::system::error_code ec;
      m_socket.cancel(ec);
      co_return make_error_code(RtspErrc::response_timeout);
    }

    auto bytes = std::get<0>(result);
    const auto* src = reinterpret_cast<const uint8_t*>(buffer.data());
    m_recv_buffer.insert(m_recv_buffer.end(), src, src + bytes);
    co_return std::error_code{};
  }
  catch (const boost::system::system_error& e) {
    if (e.code() == boost::asio::error::eof) {
      m_connected = false;
      co_return make_error_code(RtspErrc::connection_closed);
    }
    m_connected = false;
    co_return make_error_code(RtspErrc::receive_failed);
  }
}

} // namespace nx::net
