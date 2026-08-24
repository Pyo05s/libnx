// 파일: datetime_unittest.cpp
// 생성일: 2026-02-19
// 설명: DateTime ISO 8601 변환 단위 테스트

#include <nxnet/onvif/onvif_types.h>

#include <gtest/gtest.h>
#include <nxcore/util/time_util.h>
#include <ctime>

namespace nx::net::onvif {

// ============================================================================
// to_iso8601 테스트
// ============================================================================

TEST(DateTimeTest, ToIso8601_UtcTime)
{
  // UTC 시간을 ISO 8601 형식으로 변환 검증
  DateTime dt{.year = 2026, .month = 2, .day = 19, .hour = 10, .minute = 30, .second = 0};

  std::string iso = dt.to_iso8601();

  // 예상 형식: 2026-02-19T10:30:00Z
  EXPECT_EQ(iso, "2026-02-19T10:30:00Z");
}

TEST(DateTimeTest, ToIso8601_ZeroPaddingMonthDay)
{
  // 자릿수 패딩 검증 (월/일/시/분/초 2자리)
  DateTime dt{.year = 2026, .month = 1, .day = 5, .hour = 8, .minute = 3, .second = 7};

  std::string iso = dt.to_iso8601();

  // 예상 형식: 2026-01-05T08:03:07Z
  EXPECT_EQ(iso, "2026-01-05T08:03:07Z");
}

// ============================================================================
// from_iso8601 테스트
// ============================================================================

TEST(DateTimeTest, FromIso8601_ValidString)
{
  // ISO 8601 문자열 파싱 후 각 필드 검증
  auto result = DateTime::from_iso8601("2026-02-19T10:30:00Z");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->year, 2026);
  EXPECT_EQ(result->month, 2);
  EXPECT_EQ(result->day, 19);
  EXPECT_EQ(result->hour, 10);
  EXPECT_EQ(result->minute, 30);
  EXPECT_EQ(result->second, 0);
}

TEST(DateTimeTest, FromIso8601_InvalidString)
{
  // 잘못된 형식 문자열 입력 시 nullopt 반환 검증
  auto result = DateTime::from_iso8601("not-a-valid-datetime");
  EXPECT_FALSE(result.has_value());

  // 빈 문자열
  auto result2 = DateTime::from_iso8601("");
  EXPECT_FALSE(result2.has_value());

  // 완전히 다른 형식의 날짜 문자열 (ISO 8601 형식 아님)
  auto result3 = DateTime::from_iso8601("19/02/2026 10:30:00");
  EXPECT_FALSE(result3.has_value());
}

// ============================================================================
// now_utc 테스트
// ============================================================================

TEST(DateTimeTest, NowUtc_ReturnsCurrentTime)
{
  // 현재 UTC 시간을 반환하고 허용 오차 ±2초 이내인지 검증
  DateTime now = DateTime::now_utc();

  // 예상 연도 범위 검증 (2020 이상)
  EXPECT_GE(now.year, 2020);

  // std::chrono를 이용한 현재 시각과 비교 (±2초 허용)
  auto sys_now = std::chrono::system_clock::now();
  auto dt_tp = now.to_time_point();
  auto diff = std::chrono::abs(sys_now - dt_tp);
  auto diff_secs = nx::duration_count<nx::seconds>(diff);

  EXPECT_LE(diff_secs, 2) << "현재 UTC 시각과 ±2초 이상 차이남";
}

TEST(DateTimeTest, NowUtc_ValidFields)
{
  // now_utc() 반환값의 각 필드 유효 범위 검증
  DateTime now = DateTime::now_utc();

  EXPECT_GE(now.year, 2020);
  EXPECT_GE(now.month, 1);
  EXPECT_LE(now.month, 12);
  EXPECT_GE(now.day, 1);
  EXPECT_LE(now.day, 31);
  EXPECT_GE(now.hour, 0);
  EXPECT_LE(now.hour, 23);
  EXPECT_GE(now.minute, 0);
  EXPECT_LE(now.minute, 59);
  EXPECT_GE(now.second, 0);
  EXPECT_LE(now.second, 59);
}

// ============================================================================
// 왕복 변환 테스트
// ============================================================================

TEST(DateTimeTest, RoundTrip_ToAndFrom)
{
  // to_iso8601 → from_iso8601 왕복 변환 동일성 검증
  DateTime
    original{.year = 2026, .month = 6, .day = 15, .hour = 14, .minute = 22, .second = 55};

  std::string iso = original.to_iso8601();
  auto parsed = DateTime::from_iso8601(iso);

  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->year, original.year);
  EXPECT_EQ(parsed->month, original.month);
  EXPECT_EQ(parsed->day, original.day);
  EXPECT_EQ(parsed->hour, original.hour);
  EXPECT_EQ(parsed->minute, original.minute);
  EXPECT_EQ(parsed->second, original.second);
}

TEST(DateTimeTest, RoundTrip_MultipleValues)
{
  // 다양한 날짜/시간값으로 왕복 변환 동일성 검증
  const std::vector<DateTime> test_cases = {
    DateTime{.year = 2020,.month = 1,.day = 1,.hour = 0,.minute = 0,.second = 0                                                                           },
    DateTime{
             .year = 2025,
             .month = 12,
             .day = 31,
             .hour = 23,
             .minute = 59,
             .second = 59                                                              },
    DateTime{.year = 2026, .month = 7,  .day = 4,  .hour = 12, .minute = 0, .second = 0}
  };

  for (const auto& original : test_cases) {
    std::string iso = original.to_iso8601();
    auto parsed = DateTime::from_iso8601(iso);

    ASSERT_TRUE(parsed.has_value()) << "파싱 실패: " << iso;
    EXPECT_EQ(parsed->year, original.year) << "연도 불일치: " << iso;
    EXPECT_EQ(parsed->month, original.month) << "월 불일치: " << iso;
    EXPECT_EQ(parsed->day, original.day) << "일 불일치: " << iso;
    EXPECT_EQ(parsed->hour, original.hour) << "시 불일치: " << iso;
    EXPECT_EQ(parsed->minute, original.minute) << "분 불일치: " << iso;
    EXPECT_EQ(parsed->second, original.second) << "초 불일치: " << iso;
  }
}

} // namespace nx::net::onvif
