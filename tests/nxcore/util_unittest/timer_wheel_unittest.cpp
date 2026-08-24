// 파일: timer_wheel_unittest.cpp
// 생성일: 2026-03-31
// 설명: TimerWheel 유닛 테스트

#include <nxcore/util/timer_wheel.h>
#include <nxcore/util/time_util.h>
#include <gtest/gtest.h>
#include <nxcore/util/asio_type.h>

#include "coroutine_helper.h"
#include "io_context_test_runner.h"

#include <atomic>
#include <thread>

using namespace nx::util;

namespace {

class TimerWheelTest : public ::testing::Test
{
protected:
  void SetUp() override { m_runner.start(2); }

  void TearDown() override { m_runner.stop(); }

  test::IoContextTestRunner m_runner;
};

// 기본 생성 및 시작/정지
TEST_F(TimerWheelTest, StartAndStop)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(100));
    EXPECT_FALSE(wheel->is_running());
    EXPECT_EQ(wheel->size(), 0);

    wheel->start();
    EXPECT_TRUE(wheel->is_running());

    co_await wheel->stop();
    EXPECT_FALSE(wheel->is_running());
  }());
}

// 콜백 등록 및 실행 확인
TEST_F(TimerWheelTest, CallbackFires)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(50));
    wheel->start();

    std::atomic<int> count{0};
    auto handle = wheel->add(nx::milliseconds(100), [&count]() { count.fetch_add(1); });

    EXPECT_NE(handle, kInvalidTimerHandle);
    EXPECT_EQ(wheel->size(), 1);

    // 350ms 대기 → 100ms 간격이므로 2~3회 실행 예상
    AsioSteadyTimer timer(m_runner.io_context());
    timer.expires_after(nx::milliseconds(350));
    co_await timer.async_wait(boost::asio::use_awaitable);

    EXPECT_GE(count.load(), 2);
    EXPECT_LE(count.load(), 4);

    co_await wheel->stop();
  }());
}

// 여러 콜백 등록
TEST_F(TimerWheelTest, MultipleCallbacks)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(50));
    wheel->start();

    std::atomic<int> fast_count{0};
    std::atomic<int> slow_count{0};

    auto fast_handle
      = wheel->add(nx::milliseconds(100), [&fast_count]() { fast_count.fetch_add(1); });

    auto slow_handle
      = wheel->add(nx::milliseconds(250), [&slow_count]() { slow_count.fetch_add(1); });

    (void)fast_handle;
    (void)slow_handle;

    EXPECT_EQ(wheel->size(), 2);

    // 550ms 대기
    AsioSteadyTimer timer(m_runner.io_context());
    timer.expires_after(nx::milliseconds(550));
    co_await timer.async_wait(boost::asio::use_awaitable);

    // fast: 100ms 간격 → 4~6회
    EXPECT_GE(fast_count.load(), 3);
    // slow: 250ms 간격 → 1~2회
    EXPECT_GE(slow_count.load(), 1);

    co_await wheel->stop();
  }());
}

// 취소 테스트
TEST_F(TimerWheelTest, CancelTimer)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(50));
    wheel->start();

    std::atomic<int> count{0};
    auto handle = wheel->add(nx::milliseconds(100), [&count]() { count.fetch_add(1); });

    EXPECT_EQ(wheel->size(), 1);

    // 즉시 취소
    wheel->cancel(handle);
    EXPECT_EQ(wheel->size(), 0);

    // 300ms 대기 → 콜백 미실행 확인
    AsioSteadyTimer timer(m_runner.io_context());
    timer.expires_after(nx::milliseconds(300));
    co_await timer.async_wait(boost::asio::use_awaitable);

    EXPECT_EQ(count.load(), 0);

    co_await wheel->stop();
  }());
}

// 잘못된 핸들 취소는 안전하게 무시
TEST_F(TimerWheelTest, CancelInvalidHandle)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(100));
    wheel->start();

    // 유효하지 않은 핸들 취소
    wheel->cancel(kInvalidTimerHandle);
    wheel->cancel(9999);

    EXPECT_EQ(wheel->size(), 0);

    co_await wheel->stop();
  }());
}

// 잘못된 인자로 add() 시 kInvalidTimerHandle 반환
TEST_F(TimerWheelTest, AddInvalidArgs)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(100));
    wheel->start();

    // null 콜백
    auto h1 = wheel->add(nx::milliseconds(100), nullptr);
    EXPECT_EQ(h1, kInvalidTimerHandle);

    // 0 간격
    auto h2 = wheel->add(nx::milliseconds(0), []() {});
    EXPECT_EQ(h2, kInvalidTimerHandle);

    // 음수 간격
    auto h3 = wheel->add(nx::milliseconds(-100), []() {});
    EXPECT_EQ(h3, kInvalidTimerHandle);

    EXPECT_EQ(wheel->size(), 0);

    co_await wheel->stop();
  }());
}

// resolution 조회
TEST_F(TimerWheelTest, Resolution)
{
  auto wheel = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(200));
  EXPECT_EQ(wheel->resolution(), nx::milliseconds(200));
}

// 중복 start/stop 안전성
TEST_F(TimerWheelTest, DoubleStartStop)
{
  m_runner.run_sync([this]() -> nx::awaitable<void> {
    auto wheel
      = std::make_shared<TimerWheel>(m_runner.io_context(), nx::milliseconds(100));

    wheel->start();
    wheel->start(); // 중복 시작 무시
    EXPECT_TRUE(wheel->is_running());

    co_await wheel->stop();
    co_await wheel->stop(); // 중복 정지 무시
    EXPECT_FALSE(wheel->is_running());
  }());
}

} // namespace
