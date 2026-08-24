// 파일: scoped_perf_timer.h
// 생성일: 2026-04-22
// 설명: 진단용 RAII 성능 측정 유틸리티 (임시 프로파일링용)

#pragma once

#include <nxcore/util/time_util.h>

#include <atomic>
#include <cstdint>
#include <string_view>

#include <spdlog/spdlog.h>

namespace nx::diag {

/// 함수별 누적 통계 (lock-free)
struct PerfStats
{
  std::atomic<uint64_t> call_count{0};
  std::atomic<uint64_t> total_us{0};
  std::atomic<uint64_t> max_us{0};

  void record(uint64_t elapsed_us)
  {
    call_count.fetch_add(1, std::memory_order_relaxed);
    total_us.fetch_add(elapsed_us, std::memory_order_relaxed);

    // max 갱신 (CAS 루프)
    auto cur = max_us.load(std::memory_order_relaxed);
    while (elapsed_us > cur) {
      if (max_us.compare_exchange_weak(cur, elapsed_us, std::memory_order_relaxed)) {
        break;
      }
    }
  }

  /// 통계 출력 후 리셋 (주기적 호출용)
  void dump_and_reset(std::string_view label)
  {
    auto count = call_count.exchange(0, std::memory_order_relaxed);
    auto total = total_us.exchange(0, std::memory_order_relaxed);
    auto peak = max_us.exchange(0, std::memory_order_relaxed);

    if (count == 0) {
      return;
    }

    auto avg = total / count;
    spdlog::info(
      "[PerfStats] {} | calls={}, avg={}us, max={}us, total={}ms",
      label,
      static_cast<uint64_t>(count),
      static_cast<uint64_t>(avg),
      static_cast<uint64_t>(peak),
      static_cast<uint64_t>(total / 1000));
  }
};

/// RAII 스코프 타이머 — 블록 종료 시 PerfStats에 기록
class ScopedPerfTimer
{
public:
  explicit ScopedPerfTimer(PerfStats& stats)
      : m_stats(stats)
      , m_start(std::chrono::steady_clock::now())
  {}

  ~ScopedPerfTimer()
  {
    auto elapsed = std::chrono::steady_clock::now() - m_start;
    auto us = nx::duration_count<nx::microseconds>(elapsed);
    m_stats.record(static_cast<uint64_t>(us));
  }

  ScopedPerfTimer(const ScopedPerfTimer&) = delete;
  ScopedPerfTimer& operator=(const ScopedPerfTimer&) = delete;

private:
  PerfStats& m_stats;
  std::chrono::steady_clock::time_point m_start;
};

/// 주기적 통계 덤프 헬퍼 (코루틴 루프에서 사용)
/// 예: if (perf_tick(last_dump, 5s)) { stats.dump_and_reset("label"); }
inline bool
perf_tick(
  std::chrono::steady_clock::time_point& last, nx::seconds interval = nx::seconds(5))
{
  auto now = std::chrono::steady_clock::now();
  if (now - last >= interval) {
    last = now;
    return true;
  }
  return false;
}

} // namespace nx::diag

/// 한 줄 매크로 — static PerfStats + ScopedPerfTimer 자동 생성
/// 사용: NX_PERF_SCOPE("MyFunc") 를 함수 첫 줄에 삽입
#define NX_PERF_SCOPE(label)                                                             \
  static nx::diag::PerfStats s_perf_##__LINE__;                                          \
  nx::diag::ScopedPerfTimer _perf_timer_##__LINE__(s_perf_##__LINE__)

/// 주기적 덤프 매크로 (5초마다)
/// 사용: NX_PERF_DUMP("MyFunc", s_perf_stats) 를 루프 내에 삽입
#define NX_PERF_DUMP(label, stats)                                                       \
  do {                                                                                   \
    static auto _last_dump = std::chrono::steady_clock::now();                           \
    if (nx::diag::perf_tick(_last_dump)) {                                               \
      (stats).dump_and_reset(label);                                                     \
    }                                                                                    \
  } while (false)
