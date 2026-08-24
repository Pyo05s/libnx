// 파일: time_util.h
// 생성일: 2025-12-01
// 설명: 플랫폼에 따라 thread-safe한 시간 변환 및 timestamp 관리 기능 제공
//
// 주의사항:
// - Windows에서는 1970-01-01 00:00:00 UTC 이전 날짜(음수 timestamp) 변환이 지원되지
// 않습니다.
// - Unix/Linux/macOS에서는 1970년 이전 날짜도 정상적으로 변환됩니다.
// - 실무에서는 1970년 이후 날짜만 사용하는 것을 권장합니다.

#pragma once

#include <nxcore/util/type_util.h>

#include <ctime>
#include <cerrno>
#include <chrono>
#include <string>
#include <cstdint>
#include <expected>
#include <system_error>
#include <optional>

namespace nx {

// 시간 변환 관련 오류 코드
enum class TimeError
{
  Success = 0,
  ConversionFailed = 1,    // 시간 변환 실패
  InvalidTimestamp = 2,    // 유효하지 않은 timestamp
  ParseError = 3,          // 문자열 파싱 실패
  PlatformNotSupported = 4 // 플랫폼이 해당 기능을 지원하지 않음
};

// TimeError를 위한 error_category
class TimeErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "time_error"; }

  std::string message(int ev) const override
  {
    switch (static_cast<TimeError>(ev)) {
      case TimeError::Success: return "Success";
      case TimeError::ConversionFailed: return "Time conversion failed";
      case TimeError::InvalidTimestamp: return "Invalid timestamp value";
      case TimeError::ParseError: return "Failed to parse time string";
      case TimeError::PlatformNotSupported:
        return "Platform does not support this time operation";
      default: return "Unknown time error";
    }
  }
};

// TimeError 카테고리 싱글톤
inline const std::error_category&
time_error_category()
{
  static TimeErrorCategory instance;
  return instance;
}

// TimeError를 std::error_code로 변환
inline std::error_code
make_error_code(TimeError e)
{
  return std::error_code(static_cast<int>(e), time_error_category());
}

// 밀리초 단위 timestamp 타입 정의: Unix Epoch(1970-01-01 00:00:00 UTC)부터의 밀리초
// 별도의 local 등 표현이 없다면 항상 UTC 기준으로 간주합니다.
using mstime_t = int64_t;

// std::tm + milliseconds를 표현하는 구조체
struct DateTime
{
  std::tm tm;       // 날짜/시간 구성요소
  int milliseconds; // 밀리초 (0-999)

  DateTime()
      : tm{}
      , milliseconds(0)
  {}
};

// JavaScript Date 클래스와 유사한 timestamp 관리 클래스
class Timestamp
{
public:
  // 기본 생성자: 현재 시각(UTC)
  Timestamp();

  // timestamp(밀리초)로부터 생성
  explicit Timestamp(mstime_t ms);

  // std::tm으로부터 생성 (UTC 기준)
  // 변환 실패 시 예외 발생
  explicit Timestamp(const std::tm& tm, int ms = 0);

  // DateTime으로부터 생성 (UTC 기준)
  // 변환 실패 시 예외 발생
  explicit Timestamp(const DateTime& dt);

  // 문자열 파싱 (ISO 8601 형식 지원)
  // 예: "2025-12-01T12:34:56.789Z" 또는 "2025-12-01 12:34:56.789"
  // 반환: 성공 시 Timestamp, 실패 시 ParseError
  static nx::expected<Timestamp> parse(const std::string& str);

  // 현재 시각의 timestamp 생성
  static Timestamp now();

  // timestamp 값 가져오기/설정
  mstime_t value() const { return m_ms; }

  // timestamp 값 설정 (캐시 무효화)
  void set_value(mstime_t ms)
  {
    m_ms = ms;
    invalidate_cache();
  }

  // UTC 기준 DateTime으로 변환
  nx::expected<DateTime> to_datetime() const;

  // 로컬 시간 기준 DateTime으로 변환
  nx::expected<DateTime> to_local_datetime() const;

  // ISO 8601 형식 문자열로 변환 (UTC)
  // 예: "2025-12-01T12:34:56.789Z"
  nx::expected<std::string> to_iso_string() const;

  // 로컬 시간 ISO 8601 형식 문자열로 변환
  // 예: "2025-12-01T21:34:56.789+09:00"
  nx::expected<std::string> to_local_iso_string() const;

  // 사용자 정의 형식으로 변환 (UTC, strftime 형식 + .ms 지원)
  // 예: format("%Y-%m-%d %H:%M:%S.ms") -> "2025-12-01 12:34:56.789"
  nx::expected<std::string> format(const std::string& fmt) const;

  // 사용자 정의 형식으로 변환 (로컬)
  nx::expected<std::string> format_local(const std::string& fmt) const;

  inline nx::expected<std::string> to_date_string(std::string_view indicator = "") const
  {
    std::string fmt;
    fmt.reserve(6 + indicator.size() * 2);
    fmt += "%Y";
    fmt += indicator;
    fmt += "%m";
    fmt += indicator;
    fmt += "%d";
    return format(fmt);
  }
  // 시:분:초(.밀리초) 형태 문자열 반환 (UTC 기준). indicator는 구분자(기본
  // 빈문자열) 시:분:초(.밀리초 선택) 형태 문자열 반환 (UTC 기준). indicator는
  // 구분자(기본 빈문자열)
  inline nx::expected<std::string>
  to_time_string(std::string_view indicator = "", bool include_milliseconds = true) const
  {
    std::string fmt;
    fmt.reserve(8 + indicator.size() * 2 + (include_milliseconds ? 3 : 0));
    fmt += "%H";
    fmt += indicator;
    fmt += "%M";
    fmt += indicator;
    fmt += "%S";
    if (include_milliseconds) {
      fmt += ".ms";
    }
    return format(fmt);
  }
  // ===== 타임존 변환 메서드 =====

  // 특정 타임존의 DateTime으로 변환
  // 예: to_datetime_in("Asia/Seoul") → KST 기준 DateTime
  // 반환: 성공 시 DateTime, 실패 시 오류
  nx::expected<DateTime> to_datetime_in(const std::string& timezone_name) const;

  // 특정 타임존의 ISO 8601 문자열
  // 예: to_iso_string_in("Asia/Seoul") → "2025-12-01T21:34:56.789+09:00"
  // 반환: 성공 시 ISO 문자열, 실패 시 오류
  nx::expected<std::string> to_iso_string_in(const std::string& timezone_name) const;

  // 특정 타임존의 포맷 문자열
  // 예: format_in("%Y-%m-%d %H:%M:%S", "Asia/Seoul") → "2025-12-01 21:34:56"
  // 반환: 성공 시 포맷 문자열, 실패 시 오류
  nx::expected<std::string>
  format_in(const std::string& fmt, const std::string& timezone_name) const;

  // 년/월/일/시/분/초/밀리초 개별 접근 (UTC 기준)
  // 캐싱을 통해 연속 호출 시 성능 최적화
  nx::expected<int> year() const;
  nx::expected<int> month() const;       // 1-12
  nx::expected<int> day() const;         // 1-31
  nx::expected<int> hour() const;        // 0-23
  nx::expected<int> minute() const;      // 0-59
  nx::expected<int> second() const;      // 0-59
  nx::expected<int> millisecond() const; // 0-999

  // 요일 (0=Sunday, 6=Saturday)
  nx::expected<int> day_of_week() const;

  // 비교 연산자
  bool operator==(const Timestamp& other) const { return m_ms == other.m_ms; }
  bool operator!=(const Timestamp& other) const { return m_ms != other.m_ms; }
  bool operator<(const Timestamp& other) const { return m_ms < other.m_ms; }
  bool operator<=(const Timestamp& other) const { return m_ms <= other.m_ms; }
  bool operator>(const Timestamp& other) const { return m_ms > other.m_ms; }
  bool operator>=(const Timestamp& other) const { return m_ms >= other.m_ms; }

private:
  // 캐시된 DateTime을 가져오거나 새로 변환
  nx::expected<DateTime> get_cached_datetime_utc() const;

  // 캐시 무효화
  void invalidate_cache() const { m_cached_utc_datetime.reset(); }

private:
  mstime_t m_ms; // Unix Epoch부터의 밀리초

  // 캐싱된 UTC DateTime (lazy evaluation)
  mutable std::optional<DateTime> m_cached_utc_datetime;
};

// 현재 시각의 timestamp (밀리초) 가져오기
mstime_t now_ms();

namespace util {

// ===== 유틸리티 함수들 =====

// 플랫폼에 따라 안전하게 time_t -> tm 로 변환한다.
// 반환: 성공 시 true, 실패 시 false
bool safe_localtime(std::time_t t, std::tm& out);

// 플랫폼에 따라 안전하게 time_t -> tm (UTC) 로 변환한다.
// 반환: 성공 시 true, 실패 시 false
bool safe_gmtime(std::time_t t, std::tm& out);

// timestamp를 UTC 기준 DateTime으로 변환
// 반환: 성공 시 DateTime, 실패 시 ConversionFailed
nx::expected<DateTime> timestamp_to_datetime(mstime_t ms);

// timestamp를 로컬 시간 기준 DateTime으로 변환
// 반환: 성공 시 DateTime, 실패 시 ConversionFailed
nx::expected<DateTime> timestamp_to_local_datetime(mstime_t ms);

// UTC 기준 DateTime을 timestamp로 변환
// 반환: 성공 시 mstime_t, 실패 시 ConversionFailed 또는 InvalidTimestamp
nx::expected<mstime_t> datetime_to_timestamp(const DateTime& dt);

// 로컬 시간 기준 DateTime을 timestamp로 변환
// 반환: 성공 시 mstime_t, 실패 시 ConversionFailed 또는 InvalidTimestamp
nx::expected<mstime_t> local_datetime_to_timestamp(const DateTime& dt);

// ===== 편의 함수들 =====

// 년/월/일/시/분/초로부터 UTC timestamp 생성 (밀리초)
// 반환: 성공 시 mstime_t, 실패 시 InvalidTimestamp
// 예: make_timestamp_utc(2025, 12, 1, 10, 30, 0) -> 1733053800000
inline nx::expected<mstime_t>
make_timestamp_utc(
  int year,
  int month,
  int day,
  int hour = 0,
  int minute = 0,
  int second = 0,
  int millisecond = 0)
{
  DateTime dt;
  dt.tm.tm_year = year - 1900;
  dt.tm.tm_mon = month - 1;
  dt.tm.tm_mday = day;
  dt.tm.tm_hour = hour;
  dt.tm.tm_min = minute;
  dt.tm.tm_sec = second;
  dt.tm.tm_isdst = -1;
  dt.milliseconds = millisecond;

  return datetime_to_timestamp(dt);
}

// 년/월/일/시/분/초로부터 로컬 timestamp 생성 (밀리초)
// 반환: 성공 시 mstime_t, 실패 시 InvalidTimestamp
inline nx::expected<mstime_t>
make_timestamp_local(
  int year,
  int month,
  int day,
  int hour = 0,
  int minute = 0,
  int second = 0,
  int millisecond = 0)
{
  DateTime dt;
  dt.tm.tm_year = year - 1900;
  dt.tm.tm_mon = month - 1;
  dt.tm.tm_mday = day;
  dt.tm.tm_hour = hour;
  dt.tm.tm_min = minute;
  dt.tm.tm_sec = second;
  dt.tm.tm_isdst = -1;
  dt.milliseconds = millisecond;

  return local_datetime_to_timestamp(dt);
}

// ===== 타임존 관련 함수들 (C++20) =====

// 타임존 이름으로 UTC timestamp 생성
// IANA 타임존 데이터베이스를 사용하여 로컬 시간을 UTC로 변환
// 예: make_timestamp_with_timezone(2025, 12, 1, 8, 30, 0, "Asia/Seoul")
//     → KST 2025-12-01 08:30:00 → UTC 2025-11-30 23:30:00
//
// 지원 타임존 예시:
// - "Asia/Seoul" (한국 표준시, UTC+9)
// - "America/New_York" (미국 동부, UTC-5/-4)
// - "Europe/London" (영국, UTC+0/+1)
// - "UTC" (협정 세계시)
//
// 반환: 성공 시 UTC timestamp (밀리초), 실패 시 오류
nx::expected<mstime_t> make_timestamp_with_timezone(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  const std::string& timezone_name,
  int millisecond = 0);

// UTC 오프셋으로 UTC timestamp 생성
// 타임존 오프셋을 사용하여 로컬 시간을 UTC로 변환
// 예: make_timestamp_with_offset(2025, 12, 1, 8, 30, 0, "+09:00")
//     → UTC+9 2025-12-01 08:30:00 → UTC 2025-11-30 23:30:00
//
// 오프셋 형식: "+HH:MM" 또는 "-HH:MM"
// - "+09:00" (한국, 도쿄)
// - "-05:00" (미국 동부 표준시)
// - "+00:00" 또는 "Z" (UTC)
//
// 주의: 이 함수는 DST(일광 절약 시간)를 고려하지 않습니다.
//       DST가 필요한 경우 make_timestamp_with_timezone()을 사용하세요.
//
// 반환: 성공 시 UTC timestamp (밀리초), 실패 시 오류
nx::expected<mstime_t> make_timestamp_with_offset(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  const std::string& offset_str,
  int millisecond = 0);

// UTC timestamp를 특정 타임존의 로컬 시간으로 변환
// 예: timestamp_to_datetime_timezone(utc_ms, "Asia/Seoul")
//     → KST 기준 DateTime 반환
//
// 반환: 성공 시 로컬 시간 기준 DateTime, 실패 시 오류
nx::expected<DateTime>
timestamp_to_datetime_timezone(mstime_t utc_timestamp, const std::string& timezone_name);

} // namespace util
} // namespace nx

// std::error_code를 위한 특수화
template <>
struct std::is_error_code_enum<nx::TimeError> : true_type
{};
