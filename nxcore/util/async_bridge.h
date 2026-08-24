// 파일: async_bridge.h
// 생성일: 2026-06-08
// 설명: 코루틴 awaitable을 동기 경계에서 완료까지 대기하는 브리지 유틸

#pragma once

#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <boost/asio/io_context.hpp>

#include <future>
#include <memory>
#include <stdexcept>
#include <string>
#include <type_traits>

#include <spdlog/spdlog.h>

namespace nx::async {

namespace detail {

template <typename Future>
void
wait_with_timeout(Future& future, nx::milliseconds timeout)
{
  if (timeout <= nx::milliseconds::zero()) {
    future.wait();
    return;
  }

  const auto status = future.wait_for(timeout);
  if (status == std::future_status::ready) {
    return;
  }

  spdlog::error("[async_bridge] block_on 타임아웃 발생: {}ms", timeout.count());
  throw std::runtime_error("nx::async::block_on timeout");
}

inline void
validate_call_thread(AsioContext& ioc)
{
  // io 워커 스레드에서 block_on을 호출하면 자기 완료 핸들러를 기다리며 교착될 수
  // 있다.
  if (ioc.get_executor().running_in_this_thread()) {
    spdlog::error(
      "[async_bridge] io_context 워커 스레드에서 block_on 호출 - 데드락 위험");
    throw std::logic_error("block_on must not be called from io_context worker thread");
  }
}

} // namespace detail

template <typename T>
[[nodiscard]]
T
block_on(AsioContext& ioc, nx::awaitable<T> aw, nx::milliseconds timeout)
{
  detail::validate_call_thread(ioc);

  auto promise = std::make_shared<std::promise<T>>();
  auto future = promise->get_future();

  boost::asio::co_spawn(
    ioc,
    [promise, aw = std::move(aw)]() mutable -> nx::awaitable<void> {
      try {
        if constexpr (std::is_void_v<T>) {
          co_await std::move(aw);
          promise->set_value();
        }
        else {
          promise->set_value(co_await std::move(aw));
        }
      }
      catch (...) {
        promise->set_exception(std::current_exception());
      }
      co_return;
    },
    boost::asio::detached);

  detail::wait_with_timeout(future, timeout);
  if constexpr (std::is_void_v<T>) {
    future.get();
    return;
  }
  else {
    return future.get();
  }
}

inline void
block_on(AsioContext& ioc, nx::awaitable<void> aw, nx::milliseconds timeout)
{
  block_on<void>(ioc, std::move(aw), timeout);
}

template <typename Func>
auto
dispatch_to(boost::asio::io_context& target_ioc, Func&& func)
  -> boost::asio::awaitable<std::invoke_result_t<Func>>
{
  // 현재 이 코루틴을 호출한 원래의 executor(m_ioc)를 기억합니다.
  auto current_executor = co_await boost::asio::this_coro::executor;

  // target_ioc 스레드로 전환
  co_await boost::asio::post(target_ioc, boost::asio::use_awaitable);

  // 전달받은 람다(동기 DB 작업 등) 실행
  if constexpr (std::is_void_v<std::invoke_result_t<Func>>) {
    std::forward<Func>(func)();
    // 원래 executor로 자동 복귀
    co_await boost::asio::post(current_executor, boost::asio::use_awaitable);
    co_return;
  }
  else {
    auto result = std::forward<Func>(func)();
    // 원래 executor로 자동 복귀
    co_await boost::asio::post(current_executor, boost::asio::use_awaitable);
    co_return result;
  }
}

} // namespace nx::async
