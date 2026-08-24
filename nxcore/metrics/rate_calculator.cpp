// 파일: rate_calculator.cpp
// 생성일: 2026-04-14
// 설명: delta/interval 기반 rate(변화율) 계산 유틸리티 구현

#include "nxcore/metrics/rate_calculator.h"

namespace nx::metrics {

void
RateCalculator::update(uint64_t current_value, std::chrono::steady_clock::time_point now)
{
  m_prev_value = m_curr_value;
  m_prev_time = m_curr_time;
  m_curr_value = current_value;
  m_curr_time = now;

  if (m_prev_time == std::chrono::steady_clock::time_point{}) {
    // 최초 호출: 이전 시각이 없으므로 rate 계산 불가
    m_valid = false;
    return;
  }

  auto interval = std::chrono::duration<double>(m_curr_time - m_prev_time);
  if (interval.count() <= 0.0) {
    // 시간 간격이 0 이하이면 rate 계산 불가
    m_rate_per_sec = 0.0;
    return;
  }

  // curr < prev: 세션 재연결 등으로 카운터가 리셋된 경우
  // uint64_t 실제 래핑(18 엑사바이트 이상 전송)은 현실적으로 불가능하므로
  // 리셋으로 간주하고 이번 샘플은 버린다. 다음 호출부터 새 기준점으로 계산.
  if (m_curr_value < m_prev_value) {
    m_rate_per_sec = 0.0;
    m_valid = false;
    return;
  }

  uint64_t delta = m_curr_value - m_prev_value;
  m_rate_per_sec = static_cast<double>(delta) / interval.count();
  m_valid = true;
}

double
RateCalculator::rate_per_second() const
{
  return m_rate_per_sec;
}

double
RateCalculator::rate_per_minute() const
{
  return m_rate_per_sec * 60.0;
}

nx::milliseconds
RateCalculator::elapsed() const
{
  if (m_curr_time == std::chrono::steady_clock::time_point{}) {
    return nx::milliseconds{0};
  }

  auto now = std::chrono::steady_clock::now();
  return std::chrono::duration_cast<nx::milliseconds>(now - m_curr_time);
}

bool
RateCalculator::is_valid() const
{
  return m_valid;
}

uint64_t
RateCalculator::current_value() const
{
  return m_curr_value;
}

void
RateCalculator::reset()
{
  m_prev_value = 0;
  m_curr_value = 0;
  m_prev_time = {};
  m_curr_time = {};
  m_rate_per_sec = 0.0;
  m_valid = false;
}

} // namespace nx::metrics
