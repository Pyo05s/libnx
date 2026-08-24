// 파일: timer_wheel.cpp
// 생성일: 2026-03-31
// 설명: TimerWheel 구현

#include "nxcore/util/timer_wheel.h"

#include <spdlog/spdlog.h>

namespace nx {
namespace util {

TimerWheel::TimerWheel(AsioContext& ioc, nx::milliseconds resolution)
    : m_ioc(ioc)
    , m_tick_timer(ioc)
    , m_resolution(resolution)
{}

TimerWheel::~TimerWheel()
{
  if (m_running.load()) {
    m_stop_requested.store(true);
    m_tick_timer.cancel();
  }
}

void
TimerWheel::start()
{
  if (m_running.exchange(true)) {
    return; // 이미 실행 중
  }

  m_stop_requested.store(false);

  // shared_from_this(): 반드시 make_shared<TimerWheel>()로 생성되어야 함
  boost::asio::co_spawn(
    m_ioc,
    [self = shared_from_this()]() -> nx::awaitable<void> { co_await self->tick_loop(); },
    boost::asio::detached);
}

nx::awaitable<void>
TimerWheel::stop()
{
  if (!m_running.load()) {
    co_return;
  }

  m_stop_requested.store(true);
  m_tick_timer.cancel();

  // tick_loop가 종료될 때까지 짧게 폴링
  AsioSteadyTimer wait_timer(m_ioc);
  auto start = std::chrono::steady_clock::now();
  constexpr auto timeout = nx::seconds(5);

  while (m_running.load()) {
    if (std::chrono::steady_clock::now() - start >= timeout) {
      spdlog::warn("[TimerWheel] stop() timeout - tick_loop may be stuck");
      m_running.store(false);
      break;
    }
    wait_timer.expires_after(nx::milliseconds(10));
    co_await wait_timer.async_wait(boost::asio::use_awaitable);
  }

  co_return;
}

TimerHandle
TimerWheel::add(nx::milliseconds interval, Callback callback)
{
  if (!callback || interval.count() <= 0) {
    return kInvalidTimerHandle;
  }

  std::lock_guard<std::mutex> lock(m_mutex);

  auto handle = m_next_handle++;

  Entry entry;
  entry.handle = handle;
  entry.interval = interval;
  entry.next_fire = std::chrono::steady_clock::now() + interval;
  entry.callback = std::move(callback);

  m_entries.emplace(handle, std::move(entry));

  return handle;
}

void
TimerWheel::cancel(TimerHandle handle)
{
  if (handle == kInvalidTimerHandle) {
    return;
  }

  std::lock_guard<std::mutex> lock(m_mutex);
  m_entries.erase(handle);
}

std::size_t
TimerWheel::size() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_entries.size();
}

bool
TimerWheel::is_running() const
{
  return m_running.load();
}

nx::milliseconds
TimerWheel::resolution() const
{
  return m_resolution;
}

nx::awaitable<void>
TimerWheel::tick_loop()
{
  try {
    while (!m_stop_requested.load()) {
      m_tick_timer.expires_after(m_resolution);
      co_await m_tick_timer.async_wait(boost::asio::use_awaitable);

      fire_expired();
    }
  }
  catch (const boost::system::system_error& e) {
    if (e.code() != boost::asio::error::operation_aborted) {
      spdlog::error("[TimerWheel] tick_loop error: {}", e.what());
    }
  }

  m_running.store(false);
  co_return;
}

void
TimerWheel::fire_expired()
{
  auto now = std::chrono::steady_clock::now();

  // 만료된 콜백을 수집 (lock 범위 최소화)
  std::vector<Callback> expired_callbacks;

  {
    std::lock_guard<std::mutex> lock(m_mutex);

    for (auto& [handle, entry] : m_entries) {
      if (now >= entry.next_fire) {
        expired_callbacks.push_back(entry.callback);
        // 다음 실행 시점 갱신
        entry.next_fire = now + entry.interval;
      }
    }
  }

  // 콜백 실행 (lock 밖에서)
  for (auto& cb : expired_callbacks) {
    try {
      cb();
    }
    catch (const std::exception& e) {
      spdlog::error("[TimerWheel] callback exception: {}", e.what());
    }
  }
}

} // namespace util
} // namespace nx
