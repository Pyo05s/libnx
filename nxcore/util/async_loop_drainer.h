// 파일: async_loop_drainer.h
// 생성일: 2026-06-10
// 설명: 비동기 루프 드레이너 (Async Loop Drainer)

#pragma once

#include "asio_type.h"

#include <mutex>
#include <condition_variable>
#include <atomic>

namespace nx::util {

class DrainerBase
{
public:
  virtual ~DrainerBase() = default;
  virtual void notify_started() = 0;
  virtual void notify_stopped() = 0;
};

class BlockingDrainer : public DrainerBase
{
public:
  BlockingDrainer()
      : m_in_flight(false)
  {}

  // 루프가 시작될 때 호출 (공중에 작업이 뜸)
  void notify_started() override { m_in_flight.store(true, std::memory_order_release); }

  // 루프가 완전히 끝났을 때 호출 (작업이 내려옴)
  void notify_stopped() override
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_in_flight.store(false, std::memory_order_release);
    }
    m_cv.notify_all();
  }

  // 외부에서 루프가 끝날 때까지 대기 (Drain)
  void drain()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return !m_in_flight.load(std::memory_order_acquire); });
  }

  template <typename Rep, typename Period>
  bool drain(std::chrono::duration<Rep, Period> timeout_duration)
  {
    std::unique_lock<std::mutex> lock(m_mutex);

    // wait_for는 조건(람다)이 만족되어 깨어나면 true를,
    // 조건을 만족하지 못하고 시간이 초과되면 false를 반환합니다.
    return m_cv.wait_for(lock, timeout_duration, [this] {
      return !m_in_flight.load(std::memory_order_acquire);
    });
  }

private:
  std::atomic<bool> m_in_flight;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};

class NonBlockingDrainer : public DrainerBase
{
public:
  // executor를 주입받아 채널을 초기화합니다.
  // 1개의 void 이벤트를 버퍼링할 수 있는 채널로 설정합니다.
  NonBlockingDrainer(const boost::asio::any_io_executor& executor)
      // 처음부터 먼 미래 시간으로 타이머를 초기화해 둡니다.
      : m_timer(executor, std::chrono::steady_clock::time_point::max())
      , m_in_flight(false)
  {}

  // 루프가 시작될 때 호출
  void notify_started() override
  {
    m_in_flight = true;
    // 재사용을 위해 타이머를 다시 먼 미래로 만료 시간을 늘려둡니다.
    // cancel() 을 호출하지 않으면 깨어나지 않음에 유의하세요.
    m_timer.expires_at(std::chrono::steady_clock::time_point::max());
  }

  // 루프가 완전히 끝났을 때 호출
  void notify_stopped() override
  {
    m_in_flight = false;
    // cv.notify_all() 역할: 대기 중인 타이머를 취소시켜 코루틴을 깨웁니다.
    m_timer.cancel();
  }

  // 외부에서 루프가 끝날 때까지 비동기 대기 (co_await drain())
  [[nodiscard]]
  boost::asio::awaitable<void> drain()
  {
    if (!m_in_flight) {
      co_return; // 이미 끝났다면 즉시 리턴
    }

    boost::system::error_code ec;
    // notify_stopped()에서 cancel()을 호출할 때까지 또는 오류 발생 시 까지
    // 여기서 비동기로 멈춰있습니다.
    co_await m_timer.async_wait(
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));
  }

  // 타임아웃 기능이 포함된 비동기 대기
  // 리턴타입: 지정한 시간 내에 무사히 종료되었으면 true, 타임아웃이면 false
  template <typename Rep, typename Period>
  [[nodiscard]]
  boost::asio::awaitable<bool> drain(std::chrono::duration<Rep, Period> timeout_duration)
  {
    if (!m_in_flight) {
      co_return true;
    }

    // 타임아웃을 위해 만료 시간을 '현재 시간 + timeout'으로 변경합니다.
    m_timer.expires_after(timeout_duration);

    boost::system::error_code ec;
    co_await m_timer.async_wait(
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    // [판단 로직]
    // 만약 루프가 정상 종료되어 cancel()이 호출됐다면 -> operation_aborted
    // (성공) 만약 시간이 그냥 흘러서 만료되었다면 -> 에러 없음 (타임아웃
    // 발생)
    if (ec == boost::asio::error::operation_aborted) {
      co_return true; // 타임아웃 전에 정상 종료됨
    }

    co_return false; // 시간이 초과됨 (타임아웃)
  }

private:
  AsioSteadyTimer m_timer;
  bool m_in_flight;
};

// BlockingDrainer 다음에 추가

/// reference count 기반 동기 드레이너.
/// 동시에 여러 콜백/작업이 실행 중일 때, 모든 작업이 완료될 때까지
/// drain()에서 블로킹 대기한다. DrainGuard와 함께 사용하면 RAII로 안전하게
/// 참조 카운트를 관리할 수 있다.
class BlockingMultiDrainer : public DrainerBase
{
public:
  BlockingMultiDrainer()
      : m_count(0)
  {}

  // 작업 시작 시 호출 — 카운트 증가
  void notify_started() override { m_count.fetch_add(1, std::memory_order_acq_rel); }

  // 작업 완료 시 호출 — 카운트 감소, 0이 되면 대기 중인 스레드 깨움
  void notify_stopped() override
  {
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      const auto prev = m_count.fetch_sub(1, std::memory_order_acq_rel);
      if (prev > 1)
        return; // 아직 다른 작업이 남아있음
    }
    m_cv.notify_all();
  }

  // 모든 작업이 완료될 때까지 블로킹 대기
  void drain()
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    m_cv.wait(lock, [this] { return m_count.load(std::memory_order_acquire) == 0; });
  }

  template <typename Rep, typename Period>
  bool drain(std::chrono::duration<Rep, Period> timeout_duration)
  {
    std::unique_lock<std::mutex> lock(m_mutex);
    return m_cv.wait_for(lock, timeout_duration, [this] {
      return m_count.load(std::memory_order_acquire) == 0;
    });
  }

  int32_t in_flight_count() const { return m_count.load(std::memory_order_acquire); }

private:
  std::atomic<int32_t> m_count;
  std::mutex m_mutex;
  std::condition_variable m_cv;
};

/// reference count 기반 비동기 드레이너.
/// 동시에 여러 콜백/작업이 실행 중일 때, 마지막 작업 완료 시까지
/// co_await drain()으로 비동기 대기한다.
/// 주의: NonBlockingDrainer와 마찬가지로 단일 스트랜드에서 drain()을
/// 호출해야 한다.
class NonBlockingMultiDrainer : public DrainerBase
{
public:
  explicit NonBlockingMultiDrainer(const boost::asio::any_io_executor& executor)
      : m_timer(executor, std::chrono::steady_clock::time_point::max())
      , m_count(0)
  {}

  // 작업 시작 시 호출 — 카운트 증가, 첫 진입이면 타이머를 먼 미래로 재설정
  void notify_started() override
  {
    if (m_count.fetch_add(1, std::memory_order_acq_rel) == 0) {
      m_timer.expires_at(std::chrono::steady_clock::time_point::max());
    }
  }

  // 작업 완료 시 호출 — 카운트 감소, 0이 되면 타이머를 취소하여 drain()
  // 깨움
  void notify_stopped() override
  {
    const auto prev = m_count.fetch_sub(1, std::memory_order_acq_rel);
    if (prev <= 1) {
      m_timer.cancel();
    }
  }

  // 모든 작업이 완료될 때까지 비동기 대기
  [[nodiscard]]
  boost::asio::awaitable<void> drain()
  {
    if (m_count.load(std::memory_order_acquire) == 0) {
      co_return;
    }
    boost::system::error_code ec;
    co_await m_timer.async_wait(
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));
  }

  template <typename Rep, typename Period>
  [[nodiscard]]
  boost::asio::awaitable<bool> drain(std::chrono::duration<Rep, Period> timeout_duration)
  {
    if (m_count.load(std::memory_order_acquire) == 0) {
      co_return true;
    }
    m_timer.expires_after(timeout_duration);
    boost::system::error_code ec;
    co_await m_timer.async_wait(
      boost::asio::redirect_error(boost::asio::use_awaitable, ec));

    if (ec == boost::asio::error::operation_aborted) {
      co_return true; // 모든 작업 완료 후 cancel()
    }
    co_return false; // 타임아웃
  }

  int32_t in_flight_count() const { return m_count.load(std::memory_order_acquire); }

private:
  AsioSteadyTimer m_timer;
  std::atomic<int32_t> m_count;
};

// 안전한 예외 처리를 위한 RAII 가드는 그대로 유지합니다.
class [[nodiscard]] DrainGuard
{
public:
  explicit DrainGuard(DrainerBase& drainer)
      : m_drainer(drainer)
  {
    m_drainer.notify_started();
  }

  ~DrainGuard() { m_drainer.notify_stopped(); }

  DrainGuard(const DrainGuard&) = delete;
  DrainGuard& operator=(const DrainGuard&) = delete;

private:
  DrainerBase& m_drainer;
};
} // namespace nx::util