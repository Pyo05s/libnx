// 파일: time_util_unittest.cpp
// 생성일: 2025-12-01
// 설명: time_util의 timestamp 및 시간 변환 기능 테스트

#include <nxcore/util/time_util.h>
#include <gtest/gtest.h>
#include <thread>

using namespace nx;

TEST(TimeUtilTest, NowMs)
{
  mstime_t ts1 = now_ms();
  std::this_thread::sleep_for(nx::milliseconds(10));
  mstime_t ts2 = now_ms();

  EXPECT_GT(ts2, ts1);
  EXPECT_GE(ts2 - ts1, 10);
}

TEST(TimestampTest, DefaultConstructor)
{
  Timestamp ts1;
  std::this_thread::sleep_for(nx::milliseconds(10));
  Timestamp ts2;

  EXPECT_LT(ts1.value(), ts2.value());
}

TEST(TimestampTest, ConstructorFromValue)
{
  mstime_t value = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(value);

  EXPECT_EQ(ts.value(), value);
}

TEST(TimestampTest, NowStatic)
{
  Timestamp ts1 = Timestamp::now();
  std::this_thread::sleep_for(nx::milliseconds(10));
  Timestamp ts2 = Timestamp::now();

  EXPECT_LT(ts1.value(), ts2.value());
}

TEST(TimestampTest, ToDateTimeUtc)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms);

  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2024);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 54);
  EXPECT_EQ(dt.tm.tm_sec, 56);
  EXPECT_EQ(dt.milliseconds, 789);
}

TEST(TimestampTest, ConstructorFromTm)
{
  std::tm tm = {};
  tm.tm_year = 2024 - 1900;
  tm.tm_mon = 12 - 1;
  tm.tm_mday = 1;
  tm.tm_hour = 23;
  tm.tm_min = 54;
  tm.tm_sec = 56;

  Timestamp ts(tm, 789);

  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2024);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 54);
  EXPECT_EQ(dt.tm.tm_sec, 56);
  EXPECT_EQ(dt.milliseconds, 789);
}

TEST(TimestampTest, ConstructorFromDateTime)
{
  DateTime dt;
  dt.tm.tm_year = 2024 - 1900;
  dt.tm.tm_mon = 12 - 1;
  dt.tm.tm_mday = 1;
  dt.tm.tm_hour = 23;
  dt.tm.tm_min = 54;
  dt.tm.tm_sec = 56;
  dt.milliseconds = 789;

  Timestamp ts(dt);

  auto dt2_result = ts.to_datetime();
  ASSERT_TRUE(dt2_result.has_value());

  const DateTime& dt2 = dt2_result.value();
  EXPECT_EQ(dt2.tm.tm_year, dt.tm.tm_year);
  EXPECT_EQ(dt2.tm.tm_mon, dt.tm.tm_mon);
  EXPECT_EQ(dt2.tm.tm_mday, dt.tm.tm_mday);
  EXPECT_EQ(dt2.tm.tm_hour, dt.tm.tm_hour);
  EXPECT_EQ(dt2.tm.tm_min, dt.tm.tm_min);
  EXPECT_EQ(dt2.tm.tm_sec, dt.tm.tm_sec);
  EXPECT_EQ(dt2.milliseconds, dt.milliseconds);
}

TEST(TimestampTest, ToIsoString)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms);

  auto iso_result = ts.to_iso_string();
  ASSERT_TRUE(iso_result.has_value());

  const std::string& iso = iso_result.value();
  EXPECT_NE(iso.find("2024-12-01T"), std::string::npos);
  EXPECT_NE(iso.find("23:54:56"), std::string::npos);
  EXPECT_NE(iso.find(".789Z"), std::string::npos);
}

TEST(TimestampTest, Parse)
{
  std::string iso = "2024-12-01T23:54:56.789Z";
  auto ts_result = Timestamp::parse(iso);
  ASSERT_TRUE(ts_result.has_value());

  Timestamp ts = ts_result.value();
  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2024);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 54);
  EXPECT_EQ(dt.tm.tm_sec, 56);
  EXPECT_EQ(dt.milliseconds, 789);
}

TEST(TimestampTest, ParseWithoutMilliseconds)
{
  std::string iso = "2024-12-01T23:54:56Z";
  auto ts_result = Timestamp::parse(iso);
  ASSERT_TRUE(ts_result.has_value());

  Timestamp ts = ts_result.value();
  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2024);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 54);
  EXPECT_EQ(dt.tm.tm_sec, 56);
}

TEST(TimestampTest, ParseError)
{
  std::string invalid_iso = "invalid-date-string";
  auto ts_result = Timestamp::parse(invalid_iso);
  ASSERT_FALSE(ts_result.has_value());
  EXPECT_EQ(ts_result.error(), make_error_code(TimeError::ParseError));
}

TEST(TimestampTest, Format)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms);

  auto formatted_result = ts.format("%Y-%m-%d %H:%M:%S.ms");
  ASSERT_TRUE(formatted_result.has_value());

  const std::string& formatted = formatted_result.value();
  EXPECT_NE(formatted.find("2024-12-01"), std::string::npos);
  EXPECT_NE(formatted.find("23:54:56"), std::string::npos);
  EXPECT_NE(formatted.find(".789"), std::string::npos);
}

TEST(TimestampTest, IndividualFields)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms);

  auto year_result = ts.year();
  ASSERT_TRUE(year_result.has_value());
  EXPECT_EQ(year_result.value(), 2024);

  auto month_result = ts.month();
  ASSERT_TRUE(month_result.has_value());
  EXPECT_EQ(month_result.value(), 12);

  auto day_result = ts.day();
  ASSERT_TRUE(day_result.has_value());
  EXPECT_EQ(day_result.value(), 1);

  auto hour_result = ts.hour();
  ASSERT_TRUE(hour_result.has_value());
  EXPECT_EQ(hour_result.value(), 23);

  auto minute_result = ts.minute();
  ASSERT_TRUE(minute_result.has_value());
  EXPECT_EQ(minute_result.value(), 54);

  auto second_result = ts.second();
  ASSERT_TRUE(second_result.has_value());
  EXPECT_EQ(second_result.value(), 56);

  auto millisecond_result = ts.millisecond();
  ASSERT_TRUE(millisecond_result.has_value());
  EXPECT_EQ(millisecond_result.value(), 789);
}

TEST(TimestampTest, DayOfWeek)
{
  std::tm tm = {};
  tm.tm_year = 2024 - 1900;
  tm.tm_mon = 12 - 1;
  tm.tm_mday = 1;
  tm.tm_hour = 0;
  tm.tm_min = 0;
  tm.tm_sec = 0;

  Timestamp ts(tm, 0);

  auto wday_result = ts.day_of_week();
  ASSERT_TRUE(wday_result.has_value());

  int wday = wday_result.value();
  EXPECT_GE(wday, 0);
  EXPECT_LE(wday, 6);
}

TEST(TimestampTest, ComparisonOperators)
{
  Timestamp ts1(1000);
  Timestamp ts2(2000);
  Timestamp ts3(1000);

  EXPECT_TRUE(ts1 == ts3);
  EXPECT_TRUE(ts1 != ts2);
  EXPECT_TRUE(ts1 < ts2);
  EXPECT_TRUE(ts1 <= ts2);
  EXPECT_TRUE(ts1 <= ts3);
  EXPECT_TRUE(ts2 > ts1);
  EXPECT_TRUE(ts2 >= ts1);
  EXPECT_TRUE(ts1 >= ts3);
}

TEST(TimestampTest, UtcAndLocalConversion)
{
  Timestamp ts = Timestamp::now();

  auto utc_result = ts.to_datetime();
  ASSERT_TRUE(utc_result.has_value());

  auto local_result = ts.to_local_datetime();
  ASSERT_TRUE(local_result.has_value());

  const DateTime& utc = utc_result.value();
  const DateTime& local = local_result.value();

  EXPECT_GE(utc.tm.tm_hour, 0);
  EXPECT_LE(utc.tm.tm_hour, 23);
  EXPECT_GE(local.tm.tm_hour, 0);
  EXPECT_LE(local.tm.tm_hour, 23);
}

TEST(TimestampTest, IsoStringLocal)
{
  Timestamp ts = Timestamp::now();

  auto iso_local_result = ts.to_local_iso_string();
  ASSERT_TRUE(iso_local_result.has_value());

  const std::string& iso_local = iso_local_result.value();
  EXPECT_TRUE(
    iso_local.find('+') != std::string::npos || iso_local.find('-') != std::string::npos);
}

TEST(TimeUtilTest, TimestampToDateTime)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  auto dt_result = util::timestamp_to_datetime(ms);
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2024);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.milliseconds, 789);
}

TEST(TimeUtilTest, DateTimeToTimestamp)
{
  DateTime dt;
  dt.tm.tm_year = 2024 - 1900;
  dt.tm.tm_mon = 12 - 1;
  dt.tm.tm_mday = 1;
  dt.tm.tm_hour = 23;
  dt.tm.tm_min = 54;
  dt.tm.tm_sec = 56;
  dt.milliseconds = 789;

  auto ms_result = util::datetime_to_timestamp(dt);
  ASSERT_TRUE(ms_result.has_value());

  mstime_t ms = ms_result.value();
  auto dt2_result = util::timestamp_to_datetime(ms);
  ASSERT_TRUE(dt2_result.has_value());

  const DateTime& dt2 = dt2_result.value();
  EXPECT_EQ(dt2.tm.tm_year, dt.tm.tm_year);
  EXPECT_EQ(dt2.tm.tm_mon, dt.tm.tm_mon);
  EXPECT_EQ(dt2.tm.tm_mday, dt.tm.tm_mday);
  EXPECT_EQ(dt2.milliseconds, dt.milliseconds);
}

TEST(TimestampTest, EpochZero)
{
  Timestamp ts(0);

  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 1970);
  EXPECT_EQ(dt.tm.tm_mon + 1, 1);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 0);
  EXPECT_EQ(dt.tm.tm_min, 0);
  EXPECT_EQ(dt.tm.tm_sec, 0);
  EXPECT_EQ(dt.milliseconds, 0);
}

TEST(TimestampTest, NegativeTimestamp)
{
  mstime_t ms = -86400000LL;
  Timestamp ts(ms);

  EXPECT_EQ(ts.value(), ms);

#if !defined(_WIN32) && !defined(_WIN64)
  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 1969);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 31);
#else
  // Windows에서는 실패 예상
  auto dt_result = ts.to_datetime();
  // 성공 또는 실패 모두 허용
#endif
}

TEST(TimestampTest, TimestampRange)
{
  // 2000-01-01 00:00:00.000 UTC
  mstime_t ms_2000 = 946684800000LL;
  Timestamp ts(ms_2000);

  auto dt_result = ts.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  const DateTime& dt = dt_result.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2000);
  EXPECT_EQ(dt.tm.tm_mon + 1, 1);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 0);
  EXPECT_EQ(dt.tm.tm_min, 0);
  EXPECT_EQ(dt.tm.tm_sec, 0);

  // 2100-12-31 23:59:59.999 UTC
  mstime_t ms_2100 = 4133980799999LL;
  Timestamp ts2(ms_2100);

  auto dt2_result = ts2.to_datetime();
  ASSERT_TRUE(dt2_result.has_value());

  const DateTime& dt2 = dt2_result.value();
  EXPECT_EQ(dt2.tm.tm_year + 1900, 2100);
  EXPECT_EQ(dt2.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt2.tm.tm_mday, 31);
  EXPECT_EQ(dt2.milliseconds, 999);
}

TEST(TimestampTest, RoundTripConversion)
{
  mstime_t original = 1733097296789LL;
  Timestamp ts1(original);

  auto dt_result = ts1.to_datetime();
  ASSERT_TRUE(dt_result.has_value());

  Timestamp ts2(dt_result.value());

  EXPECT_EQ(ts1.value(), ts2.value());
}

TEST(TimeErrorTest, ErrorCodes)
{
  auto ec1 = make_error_code(TimeError::ConversionFailed);
  EXPECT_EQ(ec1.message(), "Time conversion failed");

  auto ec2 = make_error_code(TimeError::InvalidTimestamp);
  EXPECT_EQ(ec2.message(), "Invalid timestamp value");

  auto ec3 = make_error_code(TimeError::ParseError);
  EXPECT_EQ(ec3.message(), "Failed to parse time string");
}

// 캐싱 동작 검증 테스트
TEST(TimestampTest, CachingBehavior)
{
  mstime_t ms = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms);

  // 첫 번째 호출: 캐시 생성
  auto year1 = ts.year();
  ASSERT_TRUE(year1.has_value());
  EXPECT_EQ(year1.value(), 2024);

  // 연속 호출: 캐시 재사용 (변환 없음)
  auto month1 = ts.month();
  auto day1 = ts.day();
  auto hour1 = ts.hour();
  auto minute1 = ts.minute();
  auto second1 = ts.second();
  auto ms1 = ts.millisecond();

  ASSERT_TRUE(month1.has_value());
  ASSERT_TRUE(day1.has_value());
  ASSERT_TRUE(hour1.has_value());
  ASSERT_TRUE(minute1.has_value());
  ASSERT_TRUE(second1.has_value());
  ASSERT_TRUE(ms1.has_value());

  EXPECT_EQ(month1.value(), 12);
  EXPECT_EQ(day1.value(), 1);
  EXPECT_EQ(hour1.value(), 23);
  EXPECT_EQ(minute1.value(), 54);
  EXPECT_EQ(second1.value(), 56);
  EXPECT_EQ(ms1.value(), 789);

  // to_iso_string도 캐시 사용
  auto iso = ts.to_iso_string();
  ASSERT_TRUE(iso.has_value());
  EXPECT_NE(iso.value().find("2024-12-01T23:54:56.789Z"), std::string::npos);
}

// set_value 호출 시 캐시 무효화 검증
TEST(TimestampTest, CacheInvalidationOnSetValue)
{
  mstime_t ms1 = 1733097296789LL; // 2024-12-01 23:54:56.789 UTC
  Timestamp ts(ms1);

  // 캐시 생성
  auto year1 = ts.year();
  ASSERT_TRUE(year1.has_value());
  EXPECT_EQ(year1.value(), 2024);

  // timestamp 변경 (캐시 무효화)
  mstime_t ms2 = 946684800000LL; // 2000-01-01 00:00:00.000 UTC
  ts.set_value(ms2);

  // 새로운 값으로 다시 변환되어야 함
  auto year2 = ts.year();
  ASSERT_TRUE(year2.has_value());
  EXPECT_EQ(year2.value(), 2000);

  auto month2 = ts.month();
  auto day2 = ts.day();
  ASSERT_TRUE(month2.has_value());
  ASSERT_TRUE(day2.has_value());
  EXPECT_EQ(month2.value(), 1);
  EXPECT_EQ(day2.value(), 1);
}

// 캐싱으로 인한 성능 향상 검증 (개념적 테스트)
TEST(TimestampTest, CachingPerformanceConcept)
{
  mstime_t ms = 1733097296789LL;
  Timestamp ts(ms);

  // 연속 호출 시 모든 필드가 정상적으로 반환되는지 확인
  // (실제 성능 측정은 벤치마크로 따로 수행)
  for (int i = 0; i < 100; ++i) {
    auto year = ts.year();
    auto month = ts.month();
    auto day = ts.day();
    auto hour = ts.hour();
    auto minute = ts.minute();
    auto second = ts.second();
    auto ms_val = ts.millisecond();

    ASSERT_TRUE(year.has_value());
    ASSERT_TRUE(month.has_value());
    ASSERT_TRUE(day.has_value());
    ASSERT_TRUE(hour.has_value());
    ASSERT_TRUE(minute.has_value());
    ASSERT_TRUE(second.has_value());
    ASSERT_TRUE(ms_val.has_value());
  }
}

// ===================================================================
// 타임존 관련 테스트 (C++20)
// ===================================================================

// 타임존 이름으로 UTC timestamp 생성 테스트
TEST(TimeUtilTest, MakeTimestampWithTimezone_AsiaSeoul)
{
  // KST 2025-12-01 08:30:00 → UTC 2025-11-30 23:30:00
  auto result = util::make_timestamp_with_timezone(2025, 12, 1, 8, 30, 0, "Asia/Seoul");

  ASSERT_TRUE(result.has_value());

  // UTC로 변환하여 검증
  auto utc_dt = util::timestamp_to_datetime(result.value());
  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  // KST는 UTC+9이므로 9시간을 빼면 UTC
  // 2025-12-01 08:30:00 KST - 9시간 = 2025-11-30 23:30:00 UTC
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 11); // 전날
  EXPECT_EQ(dt.tm.tm_mday, 30);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 30);
  EXPECT_EQ(dt.tm.tm_sec, 0);
}

TEST(TimeUtilTest, MakeTimestampWithTimezone_NewYork)
{
  // EST 2025-01-15 10:00:00 → UTC 2025-01-15 15:00:00
  // (1월은 표준시, EST = UTC-5)
  auto result
    = util::make_timestamp_with_timezone(2025, 1, 15, 10, 0, 0, "America/New_York");

  ASSERT_TRUE(result.has_value());

  auto utc_dt = util::timestamp_to_datetime(result.value());
  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 1);
  EXPECT_EQ(dt.tm.tm_mday, 15);
  // EST는 UTC-5이므로 5시간을 더하면 UTC
  EXPECT_EQ(dt.tm.tm_hour, 15);
  EXPECT_EQ(dt.tm.tm_min, 0);
}

TEST(TimeUtilTest, MakeTimestampWithTimezone_UTC)
{
  // UTC 2025-06-01 12:00:00
  auto result = util::make_timestamp_with_timezone(2025, 6, 1, 12, 0, 0, "UTC");

  ASSERT_TRUE(result.has_value());

  auto utc_dt = util::timestamp_to_datetime(result.value());
  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 6);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 12);
  EXPECT_EQ(dt.tm.tm_min, 0);
}

TEST(TimeUtilTest, MakeTimestampWithTimezone_InvalidTimezone)
{
  // 존재하지 않는 타임존
  auto result
    = util::make_timestamp_with_timezone(2025, 1, 1, 0, 0, 0, "Invalid/Timezone");

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(TimeError::ParseError));
}

TEST(TimeUtilTest, MakeTimestampWithTimezone_WithMilliseconds)
{
  // 밀리초 포함 테스트
  auto result
    = util::make_timestamp_with_timezone(2025, 3, 15, 14, 30, 45, "Europe/London", 123);

  ASSERT_TRUE(result.has_value());

  // 밀리초 확인
  EXPECT_EQ(result.value() % 1000, 123);
}

// UTC 오프셋으로 timestamp 생성 테스트
TEST(TimeUtilTest, MakeTimestampWithOffset_Positive)
{
  // UTC+09:00 (KST) 2025-12-01 08:30:00
  auto result = util::make_timestamp_with_offset(2025, 12, 1, 8, 30, 0, "+09:00");

  ASSERT_TRUE(result.has_value());

  auto utc_dt = util::timestamp_to_datetime(result.value());
  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  // 2025-12-01 08:30:00 +09:00 - 9시간 = 2025-11-30 23:30:00 UTC
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 11);
  EXPECT_EQ(dt.tm.tm_mday, 30);
  EXPECT_EQ(dt.tm.tm_hour, 23);
  EXPECT_EQ(dt.tm.tm_min, 30);
}

TEST(TimeUtilTest, MakeTimestampWithOffset_Negative)
{
  // UTC-05:00 (EST) 2025-01-15 10:00:00
  auto result = util::make_timestamp_with_offset(2025, 1, 15, 10, 0, 0, "-05:00");

  ASSERT_TRUE(result.has_value());

  auto utc_dt = util::timestamp_to_datetime(result.value());
  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  // 2025-01-15 10:00:00 -05:00 + 5시간 = 2025-01-15 15:00:00 UTC
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 1);
  EXPECT_EQ(dt.tm.tm_mday, 15);
  EXPECT_EQ(dt.tm.tm_hour, 15);
}

TEST(TimeUtilTest, MakeTimestampWithOffset_UTC)
{
  // UTC (Z 또는 +00:00)
  auto result1 = util::make_timestamp_with_offset(2025, 6, 1, 12, 0, 0, "Z");

  ASSERT_TRUE(result1.has_value());

  auto result2 = util::make_timestamp_with_offset(2025, 6, 1, 12, 0, 0, "+00:00");

  ASSERT_TRUE(result2.has_value());

  // 두 결과가 같아야 함
  EXPECT_EQ(result1.value(), result2.value());
}

TEST(TimeUtilTest, MakeTimestampWithOffset_InvalidFormat)
{
  // 잘못된 오프셋 형식
  auto result1 = util::make_timestamp_with_offset(2025, 1, 1, 0, 0, 0, "invalid");

  ASSERT_FALSE(result1.has_value());

  auto result2 = util::make_timestamp_with_offset(2025, 1, 1, 0, 0, 0, "+9"); // 형식
                                                                              // 불일치

  ASSERT_FALSE(result2.has_value());
}

TEST(TimeUtilTest, MakeTimestampWithOffset_WithMilliseconds)
{
  auto result = util::make_timestamp_with_offset(2025, 3, 15, 14, 30, 45, "+09:00", 789);

  ASSERT_TRUE(result.has_value());

  // 밀리초 확인
  EXPECT_EQ(result.value() % 1000, 789);
}

// UTC timestamp를 특정 타임존의 로컬 시간으로 변환
TEST(TimeUtilTest, TimestampToDatetimeTimezone_AsiaSeoul)
{
  // UTC 2025-11-30 23:30:00 생성
  auto utc_result = util::make_timestamp_utc(2025, 11, 30, 23, 30, 0);
  ASSERT_TRUE(utc_result.has_value());

  // KST로 변환 (UTC+9)
  auto kst_result
    = util::timestamp_to_datetime_timezone(utc_result.value(), "Asia/Seoul");

  ASSERT_TRUE(kst_result.has_value());

  const DateTime& dt = kst_result.value();
  // UTC 23:30 + 9시간 = 다음날 08:30 KST
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 8);
  EXPECT_EQ(dt.tm.tm_min, 30);
}

TEST(TimeUtilTest, TimestampToDatetimeTimezone_NewYork)
{
  // UTC 2025-01-15 15:00:00 생성
  auto utc_result = util::make_timestamp_utc(2025, 1, 15, 15, 0, 0);
  ASSERT_TRUE(utc_result.has_value());

  // EST로 변환 (UTC-5)
  auto est_result
    = util::timestamp_to_datetime_timezone(utc_result.value(), "America/New_York");

  ASSERT_TRUE(est_result.has_value());

  const DateTime& dt = est_result.value();
  // UTC 15:00 - 5시간 = 10:00 EST
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 1);
  EXPECT_EQ(dt.tm.tm_mday, 15);
  EXPECT_EQ(dt.tm.tm_hour, 10);
}

TEST(TimeUtilTest, TimestampToDatetimeTimezone_UTC)
{
  // UTC 2025-06-01 12:00:00
  auto utc_result = util::make_timestamp_utc(2025, 6, 1, 12, 0, 0);
  ASSERT_TRUE(utc_result.has_value());

  // UTC로 변환 (변화 없음)
  auto utc_dt = util::timestamp_to_datetime_timezone(utc_result.value(), "UTC");

  ASSERT_TRUE(utc_dt.has_value());

  const DateTime& dt = utc_dt.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 6);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 12);
}

// 타임존 왕복 변환 테스트
TEST(TimeUtilTest, TimezoneRoundTrip)
{
  // KST 2025-12-01 08:30:00 → UTC → KST
  auto utc_ts = util::make_timestamp_with_timezone(2025, 12, 1, 8, 30, 0, "Asia/Seoul");
  ASSERT_TRUE(utc_ts.has_value());

  auto kst_dt = util::timestamp_to_datetime_timezone(utc_ts.value(), "Asia/Seoul");
  ASSERT_TRUE(kst_dt.has_value());

  // 원래 값과 같아야 함
  const DateTime& dt = kst_dt.value();
  EXPECT_EQ(dt.tm.tm_year + 1900, 2025);
  EXPECT_EQ(dt.tm.tm_mon + 1, 12);
  EXPECT_EQ(dt.tm.tm_mday, 1);
  EXPECT_EQ(dt.tm.tm_hour, 8);
  EXPECT_EQ(dt.tm.tm_min, 30);
}

// 오프셋과 타임존 이름 결과 비교
TEST(TimeUtilTest, TimezoneVsOffsetComparison)
{
  // 같은 시각을 타임존 이름과 오프셋으로 각각 생성
  auto tz_result
    = util::make_timestamp_with_timezone(2025, 12, 1, 8, 30, 0, "Asia/Seoul");

  auto offset_result = util::make_timestamp_with_offset(2025, 12, 1, 8, 30, 0, "+09:00");

  ASSERT_TRUE(tz_result.has_value());
  ASSERT_TRUE(offset_result.has_value());

  // 결과가 같아야 함 (DST가 없는 경우)
  EXPECT_EQ(tz_result.value(), offset_result.value());
}
