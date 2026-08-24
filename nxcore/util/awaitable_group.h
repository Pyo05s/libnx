// 파일: awaitable_group.h
// 생성일: 2026-05-04
// 설명: 복수의 awaitable<void>를 병렬 실행하고 전체 완료를 대기하는 유틸리티

#pragma once

#include <nxcore/util/type_util.h>
#include <nxcore/util/asio_type.h>

#include <exception>
#include <functional>
#include <vector>

namespace nx::util {

// AwaitableGroup — nx::awaitable<void> 태스크를 병렬로 실행하고
// 전체 완료를 코루틴 방식으로 대기하는 유틸리티 클래스.
//
// 사용 예:
//   auto executor = co_await boost::asio::this_coro::executor;
//   nx::util::AwaitableGroup group(executor);
//
//   for (const auto& id : session_ids)
//       group.add(destroy_session(id));
//
//   co_await group.wait_all();           // 예외 무시
//   co_await group.wait_all_or_throw();  // 첫 번째 예외 rethrow
//   auto errs = co_await group.wait_all_collect();  // 예외 전부 수집
//
// 주의:
//   - add()한 태스크는 wait_all*() 호출 시 모두 동시에 co_spawn된다.
//   - wait_all*() 호출 후 그룹이 비워지므로 재사용 시 새 그룹을 생성해야 한다.
//   - 모든 태스크가 동일한 executor에서 실행된다.
class AwaitableGroup
{
public:
  explicit AwaitableGroup(boost::asio::any_io_executor executor)
      : m_executor(std::move(executor))
  {}

  // 태스크 등록 (실행은 wait_all*() 호출 시 시작)
  void add(nx::awaitable<void> task) { m_tasks.push_back(std::move(task)); }

  // 정책 1: 모든 태스크를 병렬 실행하고 전체 완료를 기다린다.
  //         개별 태스크 예외는 로그 없이 무시한다.
  [[nodiscard]]
  nx::awaitable<void> wait_all()
  {
    co_await run_all_impl();
  }

  // 정책 2: 모든 태스크를 병렬 실행하고 전체 완료를 기다린다.
  //         예외가 하나라도 있으면 첫 번째 예외를 rethrow한다.
  //         나머지 태스크는 완료될 때까지 기다린다.
  [[nodiscard]]
  nx::awaitable<void> wait_all_or_throw()
  {
    auto errors = co_await run_all_impl();
    if (!errors.empty()) {
      std::rethrow_exception(errors.front());
    }
  }

  // 정책 3: 모든 태스크를 병렬 실행하고 전체 완료를 기다린다.
  //         발생한 모든 예외를 수집해서 반환한다.
  //         빈 벡터는 전체 성공을 의미한다.
  [[nodiscard]]
  nx::awaitable<std::vector<std::exception_ptr>> wait_all_collect()
  {
    co_return co_await run_all_impl();
  }

private:
  // 모든 태스크를 병렬 co_spawn하고 전체 완료를 channel로 기다린다.
  // 반환: 발생한 exception_ptr 목록 (성공한 태스크는 포함하지 않음)
  [[nodiscard]]
  nx::awaitable<std::vector<std::exception_ptr>> run_all_impl()
  {
    if (m_tasks.empty()) {
      co_return std::vector<std::exception_ptr>{};
    }

    const auto count = m_tasks.size();

    // channel capacity = count: 모든 완료 신호를 버퍼링해 deadlock 방지
    boost::asio::experimental::
      channel<void(boost::system::error_code, std::exception_ptr)>
        done(m_executor, count);

    for (auto& task : m_tasks) {
      boost::asio::co_spawn(
        m_executor,
        [t = std::move(task), &done]() mutable -> nx::awaitable<void> {
          std::exception_ptr eptr;
          try {
            co_await std::move(t);
          }
          catch (...) {
            eptr = std::current_exception();
          }
          // 예외 여부와 관계없이 반드시 완료 신호 전송
          co_await done.async_send(
            boost::system::error_code{},
            std::move(eptr),
            boost::asio::use_awaitable);
        },
        boost::asio::detached);
    }

    m_tasks.clear();

    // 모든 완료 신호 수신 대기
    std::vector<std::exception_ptr> errors;
    for (std::size_t i = 0; i < count; ++i) {
      auto eptr = co_await done.async_receive(boost::asio::use_awaitable);
      if (eptr) {
        errors.push_back(std::move(eptr));
      }
    }

    co_return errors;
  }

  boost::asio::any_io_executor m_executor;
  std::vector<nx::awaitable<void>> m_tasks;
};

} // namespace nx::util
