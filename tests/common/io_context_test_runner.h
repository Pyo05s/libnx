// 파일: io_context_test_runner.h
// 생성일: 2025-01-26
// 설명: 통합 테스트를 위한 io_context 관리 유틸리티

#pragma once

#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <nxcore/util/asio_type.h>
#include <thread>
#include <future>
#include <stdexcept>
#include <string>
#include <vector>

namespace test {

class IoContextTestRunner
{
public:
  IoContextTestRunner()
      : m_work_guard(m_ioc.get_executor())
  {}

  ~IoContextTestRunner() { stop(); }

  AsioContext& io_context() { return m_ioc; }

  // thread_count 개의 스레드로 io_context 실행 시작
  void start(size_t thread_count = 1)
  {
    if (!m_threads.empty()) {
      return; // 이미 실행 중
    }

    m_threads.reserve(thread_count);
    for (size_t i = 0; i < thread_count; ++i) {
      m_threads.emplace_back([this]() { m_ioc.run(); });
    }
  }

  void stop()
  {
    m_work_guard.reset();

    if (!m_ioc.stopped()) {
      m_ioc.stop();
    }

    for (auto& thread : m_threads) {
      if (thread.joinable()) {
        thread.join();
      }
    }
    m_threads.clear();

    try {
      while (m_ioc.poll() > 0) {}
    }
    catch (...) {
    }
  }

  template <typename T>
  T run_sync(nx::awaitable<T> awaitable, nx::milliseconds timeout = nx::seconds(10))
  {
    if (m_threads.empty()) {
      throw std::runtime_error(
        "io_context가 실행 중이 아닙니다. start()를 먼저 호출하세요.");
    }

    std::promise<T> promise;
    auto future = promise.get_future();

    boost::asio::co_spawn(
      m_ioc,
      [&promise, aw = std::move(awaitable)]() mutable -> nx::awaitable<void> {
        try {
          T result = co_await std::move(aw);
          promise.set_value(std::move(result));
        }
        catch (...) {
          promise.set_exception(std::current_exception());
        }
      },
      boost::asio::detached);

    // future 대기 (타임아웃 포함)
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
      throw std::runtime_error(
        "run_sync 타임아웃: " + std::to_string(timeout.count()) + "ms");
    }

    return future.get();
  }

  void run_sync(nx::awaitable<void> awaitable, nx::milliseconds timeout = nx::seconds(10))
  {
    if (m_threads.empty()) {
      throw std::runtime_error(
        "io_context가 실행 중이 아닙니다. start()를 먼저 호출하세요.");
    }

    std::promise<void> promise;
    auto future = promise.get_future();

    boost::asio::co_spawn(
      m_ioc,
      [&promise, aw = std::move(awaitable)]() mutable -> nx::awaitable<void> {
        try {
          co_await std::move(aw);
          promise.set_value();
        }
        catch (...) {
          promise.set_exception(std::current_exception());
        }
      },
      boost::asio::detached);

    // future 대기 (타임아웃 포함)
    auto status = future.wait_for(timeout);

    if (status == std::future_status::timeout) {
      throw std::runtime_error(
        "run_sync 타임아웃: " + std::to_string(timeout.count()) + "ms");
    }

    future.get();
  }

  void poll_for(nx::milliseconds duration)
  {
    auto start = std::chrono::steady_clock::now();
    while (std::chrono::steady_clock::now() - start < duration) {
      size_t handlers_run = m_ioc.poll();
      if (handlers_run == 0) {
        // 더 이상 실행할 핸들러가 없음
        break;
      }
      std::this_thread::sleep_for(nx::milliseconds(1));
    }
  }

private:
  AsioContext m_ioc;
  AsioWorkGuard m_work_guard;
  std::vector<std::thread> m_threads;
};

} // namespace test