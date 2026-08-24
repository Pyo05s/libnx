// 파일: rate_calculator_unittest.cpp
// 생성일: 2026-04-14
// 설명: RateCalculator 단위 테스트 — rate 계산 정확성, 엣지 케이스 검증

#include <gtest/gtest.h>

#include <nxcore/metrics/rate_calculator.h>

#include <nxcore/util/time_util.h>
#include <cstdint>

using nx::metrics::RateCalculator;
using Clock = std::chrono::steady_clock;

// ============================================================================
// RateCalculator 기본 동작 테스트
// ============================================================================

class RateCalculatorTest : public ::testing::Test
{
protected:
  RateCalculator m_calc;
  // 테스트용 고정 시각
  Clock::time_point m_base_time = Clock::time_point(nx::seconds(1000));
};

TEST_F(RateCalculatorTest, InitialState_NotValid)
{
  EXPECT_FALSE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 0.0);
  EXPECT_DOUBLE_EQ(m_calc.rate_per_minute(), 0.0);
  EXPECT_EQ(m_calc.current_value(), 0u);
}

TEST_F(RateCalculatorTest, SingleUpdate_NotValid)
{
  // 최초 1회 update 후에는 rate 계산 불가
  m_calc.update(100, m_base_time);

  EXPECT_FALSE(m_calc.is_valid());
  EXPECT_EQ(m_calc.current_value(), 100u);
}

TEST_F(RateCalculatorTest, TwoUpdates_ValidRate)
{
  // 1초 간격으로 100 증가 → rate = 100/sec
  m_calc.update(0, m_base_time);
  m_calc.update(100, m_base_time + nx::seconds(1));

  EXPECT_TRUE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 100.0);
  EXPECT_DOUBLE_EQ(m_calc.rate_per_minute(), 6000.0);
}

TEST_F(RateCalculatorTest, FiveSecondInterval)
{
  // 5초 간격으로 500 증가 → rate = 100/sec
  m_calc.update(1000, m_base_time);
  m_calc.update(1500, m_base_time + nx::seconds(5));

  EXPECT_TRUE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 100.0);
}

TEST_F(RateCalculatorTest, ZeroDelta_ZeroRate)
{
  // 값 변화 없음 → rate = 0
  m_calc.update(500, m_base_time);
  m_calc.update(500, m_base_time + nx::seconds(5));

  EXPECT_TRUE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 0.0);
}

TEST_F(RateCalculatorTest, ZeroInterval_ZeroRate)
{
  // 동일 시각 두 번 호출 → rate = 0 (division by zero 방지)
  m_calc.update(0, m_base_time);
  m_calc.update(100, m_base_time);

  // interval이 0이면 rate는 갱신되지 않음
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 0.0);
}

TEST_F(RateCalculatorTest, SubSecondInterval)
{
  // 500ms 간격으로 50 증가 → rate = 100/sec
  m_calc.update(0, m_base_time);
  m_calc.update(50, m_base_time + nx::milliseconds(500));

  EXPECT_TRUE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 100.0);
}

TEST_F(RateCalculatorTest, MultipleUpdates_OnlyLastPairUsed)
{
  // 3회 연속 update → 마지막 두 값 간의 rate만 유효
  m_calc.update(0, m_base_time);
  m_calc.update(100, m_base_time + nx::seconds(1));
  m_calc.update(300, m_base_time + nx::seconds(2));

  EXPECT_TRUE(m_calc.is_valid());
  // 마지막 구간: (300-100) / 1초 = 200/sec
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 200.0);
}

TEST_F(RateCalculatorTest, CounterDecrease_TreatedAsReset)
{
  // 카운터 감소는 래핑이 아니라 리셋으로 간주
  uint64_t near_max = UINT64_MAX - 50;
  m_calc.update(near_max, m_base_time);
  m_calc.update(49, m_base_time + nx::seconds(1));

  EXPECT_FALSE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 0.0);
}

TEST_F(RateCalculatorTest, Reset_ClearsState)
{
  m_calc.update(0, m_base_time);
  m_calc.update(100, m_base_time + nx::seconds(1));
  EXPECT_TRUE(m_calc.is_valid());

  m_calc.reset();

  EXPECT_FALSE(m_calc.is_valid());
  EXPECT_DOUBLE_EQ(m_calc.rate_per_second(), 0.0);
  EXPECT_EQ(m_calc.current_value(), 0u);
}

TEST_F(RateCalculatorTest, LargeValues_Precision)
{
  // 대용량 카운터 테스트 (GB 단위 바이트)
  uint64_t base = 1'000'000'000'000ULL; // 1TB
  uint64_t delta = 5'000'000'000ULL;    // 5GB in 5 sec = 1GB/sec

  m_calc.update(base, m_base_time);
  m_calc.update(base + delta, m_base_time + nx::seconds(5));

  EXPECT_TRUE(m_calc.is_valid());
  EXPECT_NEAR(m_calc.rate_per_second(), 1'000'000'000.0, 1.0);
}
