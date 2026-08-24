// 파일: timer_wheel.h
// 생성일: 2026-03-31
// 설명: 단일 OS 타이머로 다수의 주기적 타이머를 관리하는 타이머 휠

#pragma once

#include "type_util.h"
#include "time_util.h"
#include "asio_type.h"

#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <unordered_map>

namespace nx {
namespace util {

// 타이머 항목 핸들 (취소 시 사용)
using TimerHandle = uint64_t;

// 유효하지 않은 핸들 값
inline constexpr TimerHandle kInvalidTimerHandle = 0;

// ========================================================================
// TimerWheel: 단일 OS 타이머로 N개의 주기적 콜백을 관리
// ========================================================================
//
// [문제]
//   RecorderSession 1000개 → OS steady_timer 1000개
//   → OS 타이머 큐 부하, 컨텍스트 스위칭 비용 증가
//
// [해결]
//   단일 steady_timer가 resolution 간격으로 tick하며,
//   등록된 항목의 만료 여부를 확인하고 콜백 실행.
//   OS 타이머 1000개 → 1개로 감소.
//
// [스레드 안전성]
//   내부 mutex로 항목 등록/취소는 어느 스레드에서든 안전.
//   콜백은 io_context의 executor에서 실행됨.
//
// [사용 예]
//   TimerWheel wheel(ioc, nx::milliseconds(500));
//   wheel.start();
//
//   auto handle = wheel.add(nx::seconds(2), [](){ ... });
//   wheel.cancel(handle);
//
//   co_await wheel.stop();
//
/// 반드시 make_shared<TimerWheel>()로 생성 (start() 내부 co_spawn에서
/// shared_from_this() 사용)
class TimerWheel : public std::enable_shared_from_this<TimerWheel>
{
public:
  using Callback = std::function<void()>;

  // 생성자
  // @param ioc        타이머를 실행할 io_context
  // @param resolution tick 간격 (기본 500ms). 등록된 타이머의 실제 정밀도가 됨
  explicit TimerWheel(
    AsioContext& ioc, nx::milliseconds resolution = nx::milliseconds(500));

  ~TimerWheel();

  NX_NON_COPYABLE_AND_MOVABLE(TimerWheel);

  // 타이머 휠 시작 (tick 루프 코루틴 spawn)
  void start();

  // 타이머 휠 정지 (비동기)
  nx::awaitable<void> stop();

  // 주기적 타이머 등록
  // @param interval 콜백 실행 간격
  // @param callback 만료 시 호출할 함수
  // @return 핸들 (취소 시 사용). 실패 시 kInvalidTimerHandle
  TimerHandle add(nx::milliseconds interval, Callback callback);

  // 타이머 취소
  // @param handle add()에서 반환받은 핸들
  void cancel(TimerHandle handle);

  // 등록된 타이머 수
  std::size_t size() const;

  // 실행 중 여부
  bool is_running() const;

  // tick 해상도 조회
  nx::milliseconds resolution() const;

private:
  // tick 루프 코루틴
  nx::awaitable<void> tick_loop();

  // 만료된 항목들의 콜백 실행
  void fire_expired();

private:
  // 등록된 타이머 항목
  struct Entry
  {
    TimerHandle handle;
    nx::milliseconds interval;
    std::chrono::steady_clock::time_point next_fire;
    Callback callback;
  };

  AsioContext& m_ioc;
  AsioSteadyTimer m_tick_timer;
  nx::milliseconds m_resolution;

  mutable std::mutex m_mutex;
  std::unordered_map<TimerHandle, Entry> m_entries;
  uint64_t m_next_handle = 1;

  std::atomic<bool> m_running{false};
  std::atomic<bool> m_stop_requested{false};
};

} // namespace util
} // namespace nx
