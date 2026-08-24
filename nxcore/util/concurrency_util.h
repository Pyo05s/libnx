// 파일: concurrency_util.h
// 생성일: 2026-04-22
// 설명: 경량 동시성 유틸리티 (SpinLock)

#pragma once

#include <atomic>
#include <mutex>

#if defined(_MSC_VER)
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <immintrin.h>
#endif

namespace nx {
namespace util {

// BasicLockable 인터페이스를 만족하는 스핀락
//
// 매우 짧은 임계 구역(수십~수백 ns 이하)에 적합하며,
// std::lock_guard와 함께 사용할 수 있습니다.
//
// 주의:
//   - 긴 임계 구역(I/O, 메모리 할당, sleep 포함)에는 사용 금지
//   - 재귀 잠금 불가 (deadlock 발생)
//   - NUMA 환경에서 과도한 캐시 라인 경합을 유발할 수 있음
class SpinLock
{
public:
  void lock() noexcept
  {
    while (m_flag.test_and_set(std::memory_order_acquire)) {
      // CPU 파이프라인 힌트: 스핀 대기 중 다른 하이퍼스레드에 실행 기회 양보
#if defined(_MSC_VER) || defined(__x86_64__) || defined(__i386__)
      _mm_pause();
#else
      // 폴백: 컴파일러 최적화 방지용 펜스
      std::atomic_signal_fence(std::memory_order_seq_cst);
#endif
    }
  }

  void unlock() noexcept { m_flag.clear(std::memory_order_release); }

  bool try_lock() noexcept { return !m_flag.test_and_set(std::memory_order_acquire); }

private:
  std::atomic_flag m_flag = ATOMIC_FLAG_INIT;
};

} // namespace util
} // namespace nx
