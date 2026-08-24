// 파일: time_util.cpp
// 생성일: 2025-12-01
// 설명: 시간 변환 및 timestamp 관리 기능 구현

#include "time_util.h"
#include <sstream>
#include <iomanip>
#include <cstring>
#include <stdexcept>

namespace nx {

mstime_t
now_ms()
{
  auto now = std::chrono::system_clock::now();
  return static_cast<mstime_t>(
    nx::duration_count<nx::milliseconds>(now.time_since_epoch()));
}

Timestamp::Timestamp()
    : m_ms(now_ms())
{}

Timestamp::Timestamp(mstime_t ms)
    : m_ms(ms)
{}

Timestamp::Timestamp(const std::tm& tm, int ms)
{
  DateTime dt;
  dt.tm = tm;
  dt.milliseconds = ms;

  auto result = util::datetime_to_timestamp(dt);
  if (!result) {
    throw std::runtime_error(
      "Failed to convert tm to timestamp: " + result.error().message());
  }

  m_ms = result.value();
}

Timestamp::Timestamp(const DateTime& dt)
{
  auto result = util::datetime_to_timestamp(dt);
  if (!result) {
    throw std::runtime_error(
      "Failed to convert DateTime to timestamp: " + result.error().message());
  }

  m_ms = result.value();
}

Timestamp
Timestamp::now()
{
  return Timestamp(now_ms());
}

nx::expected<Timestamp>
Timestamp::parse(const std::string& str)
{
  DateTime dt;
  std::memset(&dt.tm, 0, sizeof(std::tm));
  dt.milliseconds = 0;

  int year, month, day, hour, min, sec, ms = 0;
  char sep1, sep2, sep3;

  std::istringstream iss(str);
  if (iss >> year >> sep1 >> month >> sep2 >> day >> sep3 >> hour) {
    if (iss.get() == ':' && iss >> min) {
      if (iss.get() == ':' && iss >> sec) {
        if (iss.peek() == '.') {
          iss.get();
          iss >> ms;
        }

        dt.tm.tm_year = year - 1900;
        dt.tm.tm_mon = month - 1;
        dt.tm.tm_mday = day;
        dt.tm.tm_hour = hour;
        dt.tm.tm_min = min;
        dt.tm.tm_sec = sec;
        dt.milliseconds = ms;

        try {
          return Timestamp(dt);
        }
        catch (...) {
          return std::unexpected(make_error_code(TimeError::ParseError));
        }
      }
    }
  }

  return std::unexpected(make_error_code(TimeError::ParseError));
}

nx::expected<DateTime>
Timestamp::to_datetime() const
{
  return util::timestamp_to_datetime(m_ms);
}

nx::expected<DateTime>
Timestamp::to_local_datetime() const
{
  return util::timestamp_to_local_datetime(m_ms);
}

nx::expected<DateTime>
Timestamp::get_cached_datetime_utc() const
{
  // 캐시가 있으면 재사용
  if (m_cached_utc_datetime.has_value()) {
    return m_cached_utc_datetime.value();
  }

  // 캐시가 없으면 변환 수행
  auto result = util::timestamp_to_datetime(m_ms);
  if (!result) {
    return std::unexpected(result.error());
  }

  // 캐시에 저장
  m_cached_utc_datetime = result.value();
  return result.value();
}

nx::expected<std::string>
Timestamp::to_iso_string() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << (dt.tm.tm_year + 1900) << '-'
      << std::setw(2) << (dt.tm.tm_mon + 1) << '-' << std::setw(2) << dt.tm.tm_mday << 'T'
      << std::setw(2) << dt.tm.tm_hour << ':' << std::setw(2) << dt.tm.tm_min << ':'
      << std::setw(2) << dt.tm.tm_sec << '.' << std::setw(3) << dt.milliseconds << 'Z';

  return oss.str();
}

nx::expected<std::string>
Timestamp::to_local_iso_string() const
{
  auto dt_result = to_local_datetime();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  std::time_t sec = m_ms / 1000;
  std::tm utc_tm, local_tm;

  if (!util::safe_gmtime(sec, utc_tm) || !util::safe_localtime(sec, local_tm)) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }

  std::time_t utc_time = std::mktime(&utc_tm);
  std::time_t local_time = std::mktime(&local_tm);
  int offset_sec = static_cast<int>(std::difftime(local_time, utc_time));
  int offset_hour = offset_sec / 3600;
  int offset_min = (std::abs(offset_sec) % 3600) / 60;

  std::ostringstream oss;
  oss << std::setfill('0') << std::setw(4) << (dt.tm.tm_year + 1900) << '-'
      << std::setw(2) << (dt.tm.tm_mon + 1) << '-' << std::setw(2) << dt.tm.tm_mday << 'T'
      << std::setw(2) << dt.tm.tm_hour << ':' << std::setw(2) << dt.tm.tm_min << ':'
      << std::setw(2) << dt.tm.tm_sec << '.' << std::setw(3) << dt.milliseconds
      << (offset_sec >= 0 ? '+' : '-') << std::setw(2) << std::abs(offset_hour) << ':'
      << std::setw(2) << offset_min;

  return oss.str();
}

nx::expected<std::string>
Timestamp::format(const std::string& fmt) const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  char buffer[256];
  std::strftime(buffer, sizeof(buffer), fmt.c_str(), &dt.tm);

  std::string result(buffer);

  size_t pos = result.find(".ms");
  if (pos != std::string::npos) {
    std::ostringstream oss;
    oss << '.' << std::setfill('0') << std::setw(3) << dt.milliseconds;
    result.replace(pos, 3, oss.str());
  }

  pos = fmt.find("ms");
  if (pos != std::string::npos && (pos == 0 || fmt[pos - 1] != '.')) {
    size_t result_pos = result.find("ms");
    if (result_pos != std::string::npos) {
      std::ostringstream oss;
      oss << std::setfill('0') << std::setw(3) << dt.milliseconds;
      result.replace(result_pos, 2, oss.str());
    }
  }

  return result;
}

nx::expected<std::string>
Timestamp::format_local(const std::string& fmt) const
{
  auto dt_result = to_local_datetime();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  char buffer[256];
  std::strftime(buffer, sizeof(buffer), fmt.c_str(), &dt.tm);

  std::string result(buffer);

  size_t pos = result.find(".ms");
  if (pos != std::string::npos) {
    std::ostringstream oss;
    oss << '.' << std::setfill('0') << std::setw(3) << dt.milliseconds;
    result.replace(pos, 3, oss.str());
  }

  pos = fmt.find("ms");
  if (pos != std::string::npos && (pos == 0 || fmt[pos - 1] != '.')) {
    size_t result_pos = result.find("ms");
    if (result_pos != std::string::npos) {
      std::ostringstream oss;
      oss << std::setfill('0') << std::setw(3) << dt.milliseconds;
      result.replace(result_pos, 2, oss.str());
    }
  }

  return result;
}

// ===== 타임존 변환 메서드 구현 =====

nx::expected<DateTime>
Timestamp::to_datetime_in(const std::string& timezone_name) const
{
  return util::timestamp_to_datetime_timezone(m_ms, timezone_name);
}

nx::expected<std::string>
Timestamp::to_iso_string_in(const std::string& timezone_name) const
{
  auto dt_result = to_datetime_in(timezone_name);
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  // 타임존 오프셋 계산 (C++20 chrono 사용)
  using namespace std::chrono;

  try {
    auto st = sys_time<milliseconds>{milliseconds{m_ms}};
    const time_zone* tz = locate_zone(timezone_name);
    auto zt = zoned_time{tz, st};
    auto info = zt.get_info();

    // 오프셋 계산
    auto offset = info.offset;
    int offset_sec = static_cast<int>(nx::duration_count<nx::seconds>(offset));
    int offset_hour = offset_sec / 3600;
    int offset_min = (std::abs(offset_sec) % 3600) / 60;

    std::ostringstream oss;
    oss << std::setfill('0') << std::setw(4) << (dt.tm.tm_year + 1900) << '-'
        << std::setw(2) << (dt.tm.tm_mon + 1) << '-' << std::setw(2) << dt.tm.tm_mday
        << 'T' << std::setw(2) << dt.tm.tm_hour << ':' << std::setw(2) << dt.tm.tm_min
        << ':' << std::setw(2) << dt.tm.tm_sec << '.' << std::setw(3) << dt.milliseconds
        << (offset_sec >= 0 ? '+' : '-') << std::setw(2) << std::abs(offset_hour) << ':'
        << std::setw(2) << offset_min;

    return oss.str();
  }
  catch (...) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }
}

nx::expected<std::string>
Timestamp::format_in(const std::string& fmt, const std::string& timezone_name) const
{
  auto dt_result = to_datetime_in(timezone_name);
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }

  const DateTime& dt = dt_result.value();

  char buffer[256];
  std::strftime(buffer, sizeof(buffer), fmt.c_str(), &dt.tm);

  std::string result(buffer);

  size_t pos = result.find(".ms");
  if (pos != std::string::npos) {
    std::ostringstream oss;
    oss << '.' << std::setfill('0') << std::setw(3) << dt.milliseconds;
    result.replace(pos, 3, oss.str());
  }

  pos = fmt.find("ms");
  if (pos != std::string::npos && (pos == 0 || fmt[pos - 1] != '.')) {
    size_t result_pos = result.find("ms");
    if (result_pos != std::string::npos) {
      std::ostringstream oss;
      oss << std::setfill('0') << std::setw(3) << dt.milliseconds;
      result.replace(result_pos, 2, oss.str());
    }
  }

  return result;
}

nx::expected<int>
Timestamp::year() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_year + 1900;
}

nx::expected<int>
Timestamp::month() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_mon + 1;
}

nx::expected<int>
Timestamp::day() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_mday;
}

nx::expected<int>
Timestamp::hour() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_hour;
}

nx::expected<int>
Timestamp::minute() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_min;
}

nx::expected<int>
Timestamp::second() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_sec;
}

nx::expected<int>
Timestamp::millisecond() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().milliseconds;
}

nx::expected<int>
Timestamp::day_of_week() const
{
  auto dt_result = get_cached_datetime_utc();
  if (!dt_result) {
    return std::unexpected(dt_result.error());
  }
  return dt_result.value().tm.tm_wday;
}

namespace util {

bool
safe_localtime(std::time_t t, std::tm& out)
{
#if defined(_WIN32) || defined(_WIN64)
  errno_t err = localtime_s(&out, &t);
  return err == 0;
#elif defined(__unix__) || defined(__APPLE__)
  return localtime_r(&t, &out) != nullptr;
#else
  std::tm* tmPtr = std::localtime(&t);
  if (!tmPtr) {
    return false;
  }
  out = *tmPtr;
  return true;
#endif
}

bool
safe_gmtime(std::time_t t, std::tm& out)
{
#if defined(_WIN32) || defined(_WIN64)
  errno_t err = gmtime_s(&out, &t);
  return err == 0;
#elif defined(__unix__) || defined(__APPLE__)
  return gmtime_r(&t, &out) != nullptr;
#else
  std::tm* tmPtr = std::gmtime(&t);
  if (!tmPtr) {
    return false;
  }
  out = *tmPtr;
  return true;
#endif
}

nx::expected<DateTime>
timestamp_to_datetime(mstime_t ms)
{
  DateTime result;

  std::time_t sec = ms / 1000;
  result.milliseconds = static_cast<int>(ms % 1000);
  if (result.milliseconds < 0) {
    result.milliseconds += 1000;
    sec -= 1;
  }

  if (!safe_gmtime(sec, result.tm)) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }

  return result;
}

nx::expected<DateTime>
timestamp_to_local_datetime(mstime_t ms)
{
  DateTime result;

  std::time_t sec = ms / 1000;
  result.milliseconds = static_cast<int>(ms % 1000);
  if (result.milliseconds < 0) {
    result.milliseconds += 1000;
    sec -= 1;
  }

  if (!safe_localtime(sec, result.tm)) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }

  return result;
}

nx::expected<mstime_t>
datetime_to_timestamp(const DateTime& dt)
{
  std::tm tm_copy = dt.tm;

#if defined(_WIN32) || defined(_WIN64)
  std::time_t sec = _mkgmtime(&tm_copy);
#else
  std::time_t sec = timegm(&tm_copy);
#endif

  if (sec == -1) {
    return std::unexpected(make_error_code(TimeError::InvalidTimestamp));
  }

  return static_cast<mstime_t>(sec) * 1000 + dt.milliseconds;
}

nx::expected<mstime_t>
local_datetime_to_timestamp(const DateTime& dt)
{
  std::tm tm_copy = dt.tm;

  std::time_t sec = std::mktime(&tm_copy);

  if (sec == -1) {
    return std::unexpected(make_error_code(TimeError::InvalidTimestamp));
  }

  return static_cast<mstime_t>(sec) * 1000 + dt.milliseconds;
}

// ===== 타임존 관련 함수들 구현 (C++20) =====

nx::expected<mstime_t>
make_timestamp_with_timezone(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  const std::string& timezone_name,
  int millisecond)
{
  using namespace std::chrono;

  try {
    // 1. year_month_day 생성
    auto ymd = year_month_day{
      std::chrono::year{year},
      std::chrono::month{static_cast<unsigned>(month)},
      std::chrono::day{static_cast<unsigned>(day)}};

    // 유효성 검사
    if (!ymd.ok()) {
      return std::unexpected(make_error_code(TimeError::InvalidTimestamp));
    }

    // 2. local_time 생성 (날짜 + 시간)
    auto ld = local_days{ymd};
    auto lt = ld.time_since_epoch() + nx::hours{hour} + nx::minutes{minute}
              + nx::seconds{second} + nx::milliseconds{millisecond};

    auto local_tp = local_time<nx::milliseconds>{lt};

    // 3. 타임존 적용
    const time_zone* tz = nullptr;
    try {
      tz = locate_zone(timezone_name);
    }
    catch (...) {
      return std::unexpected(make_error_code(TimeError::ParseError));
    }

    if (!tz) {
      return std::unexpected(make_error_code(TimeError::ParseError));
    }

    // 4. 로컬 시간 → UTC 변환
    auto zt = zoned_time{tz, local_tp};
    auto st = zt.get_sys_time();

    // 5. milliseconds로 변환
    auto ms = nx::duration_count<nx::milliseconds>(st.time_since_epoch());

    return static_cast<mstime_t>(ms);
  }
  catch (...) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }
}

nx::expected<mstime_t>
make_timestamp_with_offset(
  int year,
  int month,
  int day,
  int hour,
  int minute,
  int second,
  const std::string& offset_str,
  int millisecond)
{
  // 1. 오프셋 파싱
  int offset_hours = 0;
  int offset_minutes = 0;
  char sign = '+';

  // "Z" 또는 "+00:00"는 UTC
  if (offset_str == "Z" || offset_str == "z") {
    offset_hours = 0;
    offset_minutes = 0;
  }
  else {
    // "+09:00" 또는 "-05:00" 형식 파싱
#if defined(_WIN32) || defined(_WIN64)
    int parsed
      = sscanf_s(offset_str.c_str(), "%c%d:%d", &sign, 1, &offset_hours, &offset_minutes);
#else
    int parsed
      = std::sscanf(offset_str.c_str(), "%c%d:%d", &sign, &offset_hours, &offset_minutes);
#endif

    if (parsed != 3 || (sign != '+' && sign != '-')) {
      return std::unexpected(make_error_code(TimeError::ParseError));
    }
  }

  // 2. 오프셋을 밀리초로 변환
  int offset_sec = (offset_hours * 3600 + offset_minutes * 60);
  if (sign == '-') {
    offset_sec = -offset_sec;
  }

  // 3. 입력된 시간을 UTC로 해석 (DateTime 생성)
  DateTime dt;
  dt.tm.tm_year = year - 1900;
  dt.tm.tm_mon = month - 1;
  dt.tm.tm_mday = day;
  dt.tm.tm_hour = hour;
  dt.tm.tm_min = minute;
  dt.tm.tm_sec = second;
  dt.tm.tm_isdst = -1;
  dt.milliseconds = millisecond;

  // 4. UTC timestamp 생성
  auto utc_result = datetime_to_timestamp(dt);
  if (!utc_result) {
    return std::unexpected(utc_result.error());
  }

  // 5. 오프셋 적용: 로컬 시간 - 오프셋 = UTC
  // 예: KST(+09:00) 08:30 - 9시간 = UTC 23:30(전날)
  mstime_t utc_timestamp = utc_result.value() - (offset_sec * 1000);

  return utc_timestamp;
}

nx::expected<DateTime>
timestamp_to_datetime_timezone(mstime_t utc_timestamp, const std::string& timezone_name)
{
  using namespace std::chrono;

  try {
    // 1. UTC timestamp → system_clock::time_point 변환
    auto st = sys_time<nx::milliseconds>{nx::milliseconds{utc_timestamp}};

    // 2. 타임존 적용
    const time_zone* tz = nullptr;
    try {
      tz = locate_zone(timezone_name);
    }
    catch (...) {
      return std::unexpected(make_error_code(TimeError::ParseError));
    }

    if (!tz) {
      return std::unexpected(make_error_code(TimeError::ParseError));
    }

    // 3. UTC → 로컬 시간 변환
    auto zt = zoned_time{tz, st};
    auto lt = zt.get_local_time();

    // 4. local_time → std::tm 변환
    auto ld = floor<days>(lt);
    auto ymd = year_month_day{ld};
    auto tod = lt - ld;

    auto h = duration_cast<nx::hours>(tod);
    tod -= h;
    auto m = duration_cast<nx::minutes>(tod);
    tod -= m;
    auto s = duration_cast<nx::seconds>(tod);
    tod -= s;
    auto ms = duration_cast<nx::milliseconds>(tod);

    // 5. DateTime 생성
    DateTime result;
    result.tm.tm_year = static_cast<int>(ymd.year()) - 1900;
    result.tm.tm_mon = static_cast<unsigned>(ymd.month()) - 1;
    result.tm.tm_mday = static_cast<unsigned>(ymd.day());
    result.tm.tm_hour = static_cast<int>(h.count());
    result.tm.tm_min = static_cast<int>(m.count());
    result.tm.tm_sec = static_cast<int>(s.count());
    result.tm.tm_isdst = -1;
    result.milliseconds = static_cast<int>(ms.count());

    // tm_wday, tm_yday 계산
    // year_month_day로부터 직접 weekday 계산
    auto wd = weekday{sys_days{ymd}};
    result.tm.tm_wday = wd.c_encoding();

    // 연초부터의 날짜 차이 계산
    auto year_start_ymd = ymd.year() / January / 1;
    auto year_start_days = sys_days{year_start_ymd};
    auto current_days = sys_days{ymd};
    result.tm.tm_yday = static_cast<int>((current_days - year_start_days).count());

    return result;
  }
  catch (...) {
    return std::unexpected(make_error_code(TimeError::ConversionFailed));
  }
}

} // namespace util
} // namespace nx
