// 파일: onvif_types.cpp
// 생성일: 2026-02-17
// 설명: ONVIF 프로토콜 공통 타입 구현

#include "onvif_types.h"
#include <sstream>
#include <iomanip>
#include <ctime>
#include <regex>

namespace nx::net::onvif {

// ============================================================================
// DateTime 구현
// ============================================================================

std::string
DateTime::to_iso8601() const
{
  std::ostringstream oss;

  // YYYY-MM-DDTHH:MM:SS
  oss << std::setfill('0') << std::setw(4) << year << "-" << std::setw(2) << month
      << "-" << std::setw(2) << day << "T" << std::setw(2) << hour << ":"
      << std::setw(2) << minute << ":" << std::setw(2) << second;

  // 시간대 (Z 또는 +HH:MM)
  if (tz_hour == 0 && tz_minute == 0) {
    oss << "Z";
  }
  else {
    char tz_sign = (tz_hour >= 0) ? '+' : '-';
    int abs_tz_hour = std::abs(tz_hour);
    int abs_tz_minute = std::abs(tz_minute);

    oss << tz_sign << std::setw(2) << abs_tz_hour << ":" << std::setw(2)
        << abs_tz_minute;
  }

  return oss.str();
}

std::optional<DateTime>
DateTime::from_iso8601(const std::string& str)
{
  // ISO 8601 정규식: YYYY-MM-DDTHH:MM:SS[.fff](Z|±HH:MM)
  std::regex iso_regex(
    R"((\d{4})-(\d{2})-(\d{2})T(\d{2}):(\d{2}):(\d{2})(?:\.\d+)?(Z|([+-])(\d{2}):(\d{2})))");

  std::smatch match;
  if (!std::regex_match(str, match, iso_regex)) {
    return std::nullopt;
  }

  DateTime dt;
  dt.year = std::stoi(match[1].str());
  dt.month = std::stoi(match[2].str());
  dt.day = std::stoi(match[3].str());
  dt.hour = std::stoi(match[4].str());
  dt.minute = std::stoi(match[5].str());
  dt.second = std::stoi(match[6].str());

  // 시간대 파싱
  if (match[7].str() == "Z") {
    dt.tz_hour = 0;
    dt.tz_minute = 0;
  }
  else {
    int sign = (match[8].str() == "+") ? 1 : -1;
    dt.tz_hour = sign * std::stoi(match[9].str());
    dt.tz_minute = sign * std::stoi(match[10].str());
  }

  return dt;
}

DateTime
DateTime::now_utc()
{
  auto now = std::chrono::system_clock::now();
  return from_time_point(now);
}

std::chrono::system_clock::time_point
DateTime::to_time_point() const
{
  std::tm tm_struct = {};
  tm_struct.tm_year = year - 1900;
  tm_struct.tm_mon = month - 1;
  tm_struct.tm_mday = day;
  tm_struct.tm_hour = hour;
  tm_struct.tm_min = minute;
  tm_struct.tm_sec = second;

  // UTC 기준 time_t 생성
#ifdef _WIN32
  std::time_t time = _mkgmtime(&tm_struct);
#else
  std::time_t time = timegm(&tm_struct);
#endif

  // 시간대 오프셋 적용
  time -= (tz_hour * 3600 + tz_minute * 60);

  return std::chrono::system_clock::from_time_t(time);
}

DateTime
DateTime::from_time_point(const std::chrono::system_clock::time_point& tp)
{
  std::time_t time = std::chrono::system_clock::to_time_t(tp);

#ifdef _WIN32
  std::tm tm_struct;
  gmtime_s(&tm_struct, &time);
#else
  std::tm tm_struct;
  gmtime_r(&time, &tm_struct);
#endif

  DateTime dt;
  dt.year = tm_struct.tm_year + 1900;
  dt.month = tm_struct.tm_mon + 1;
  dt.day = tm_struct.tm_mday;
  dt.hour = tm_struct.tm_hour;
  dt.minute = tm_struct.tm_min;
  dt.second = tm_struct.tm_sec;
  dt.tz_hour = 0; // UTC
  dt.tz_minute = 0;

  return dt;
}

// ============================================================================
// MediaProfile 구현
// ============================================================================

int
MediaProfile::calculate_priority_score() const
{
  int score = 0;

  if (!video_encoder.has_value()) {
    return score;
  }

  const auto& encoder = video_encoder.value();

  // 1. 코덱 점수 (가장 높은 가중치)
  switch (encoder.codec) {
    case VideoCodec::kH264: score += 1000; break;
    case VideoCodec::kH265:
      score += 900; // H.265는 H.264보다 낮은 우선순위
      break;
    case VideoCodec::kMjpeg: score += 500; break;
    case VideoCodec::kMpeg4: score += 400; break;
    default: break;
  }

  // 2. 해상도 점수 (픽셀 수 기반)
  int pixels = encoder.resolution.width * encoder.resolution.height;
  score += pixels / 1000; // 1920x1080 = 2073점

  // 3. 비트레이트 점수 (Kbps 기반)
  score += encoder.bitrate / 1000; // 예: 4000Kbps = 4점

  return score;
}

} // namespace nx::net::onvif
