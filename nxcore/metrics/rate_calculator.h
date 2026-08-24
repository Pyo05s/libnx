// 파일: rate_calculator.h
// 생성일: 2026-04-14
// 설명: delta/interval 기반 rate(변화율) 계산 유틸리티

#pragma once

#include <nxcore/util/time_util.h>
#include <cstdint>

namespace nx::metrics {

/// 단일 카운터의 rate(변화율) 계산기
/// - 두 스냅샷 간 delta를 interval로 나누어 초당 변화량 산출
/// - lock-free: 외부에서 스냅샷 값을 전달받아 계산만 수행
class RateCalculator
{
public:
  /// 새 스냅샷 값을 공급하고 rate를 갱신
  /// @param current_value 현재 누적값
  /// @param now 현재 시각
  void update(uint64_t current_value, std::chrono::steady_clock::time_point now);

  /// 초당 변화율 (bytes/sec, entries/sec 등)
  double rate_per_second() const;

  /// 분당 변화율
  double rate_per_minute() const;

  /// 마지막 갱신 이후 경과 시간
  nx::milliseconds elapsed() const;

  /// 유효한 rate 값이 존재하는지 (최소 2회 update 필요)
  bool is_valid() const;

  /// 현재 누적값 조회
  uint64_t current_value() const;

  /// 상태 초기화
  void reset();

private:
  uint64_t m_prev_value = 0;
  uint64_t m_curr_value = 0;
  std::chrono::steady_clock::time_point m_prev_time{};
  std::chrono::steady_clock::time_point m_curr_time{};
  double m_rate_per_sec = 0.0;
  bool m_valid = false;
};

} // namespace nx::metrics
