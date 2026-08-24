#include "record_scheduler.h"
#include "../util/debug_util.h"
#include "../util/enum_util.h"
#include "../util/time_util.h"

#include <fstream>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

namespace nx {
namespace record {

RecordScheduler::RecordScheduler()
    : m_last_checked_time(std::chrono::system_clock::now())
{
}

RecordScheduler::~RecordScheduler() = default;

nx::expected<void>
RecordScheduler::load_schedule_from_file(const std::string& schedule_path)
{
  try {
    // JSON 파일 읽기
    std::ifstream file(schedule_path);
    if (!file.is_open()) {
      return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
    }

    json json_data = json::parse(file);
    file.close();

    // "schedules" 배열 확인
    if (!json_data.contains("schedules") || !json_data["schedules"].is_array()) {
      return std::unexpected(std::make_error_code(std::errc::invalid_argument));
    }

    // 각 schedule 객체 파싱
    for (const auto& schedule_json : json_data["schedules"]) {
      // 채널 ID (필수)
      if (!schedule_json.contains("channel_id") ||
          !schedule_json["channel_id"].is_number_unsigned()) {
        continue;
      }

      int64_t channel_id = schedule_json["channel_id"].get<int64_t>();

      // enabled 필드 (선택, 기본: true)
      bool enabled = true;
      if (schedule_json.contains("enabled") && schedule_json["enabled"].is_boolean()) {
        enabled = schedule_json["enabled"].get<bool>();
      }

      // ChannelSchedule 생성
      ChannelSchedule channel_schedule;
      channel_schedule.channel_id = channel_id;
      channel_schedule.enabled = enabled;

      // 모든 요일 초기화
      for (int d = 0; d < 7; ++d) {
        channel_schedule.weekday_schedules[d].weekday = d;
        for (int h = 0; h < 24; ++h) {
          channel_schedule.weekday_schedules[d].hourly_schedules[h].attribute =
            RecordAttribute::kNone;
        }
      }

      // weekdays 배열 파싱
      if (schedule_json.contains("weekdays") && schedule_json["weekdays"].is_array()) {
        for (const auto& weekday_json : schedule_json["weekdays"]) {
          if (!weekday_json.contains("weekday") || !weekday_json["weekday"].is_number()) {
            continue;
          }

          int weekday = weekday_json["weekday"].get<int>();
          if (weekday < 0 || weekday >= 7) {
            continue;
          }

          // hours 배열 파싱
          if (weekday_json.contains("hours") && weekday_json["hours"].is_array()) {
            for (const auto& hour_json : weekday_json["hours"]) {
              if (!hour_json.contains("hour") || !hour_json["hour"].is_number()) {
                continue;
              }

              int hour = hour_json["hour"].get<int>();
              if (hour < 0 || hour >= 24) {
                continue;
              }

              // attribute 파싱
              RecordAttribute attribute = RecordAttribute::kNone;
              if (hour_json.contains("attribute") && hour_json["attribute"].is_string()) {
                std::string attr_str = hour_json["attribute"].get<std::string>();
                attribute = parse_attribute_string(attr_str);

                // Manual 속성 필터링: 설정에서는 무시
                // Manual은 실시간 실행 중에만 부여됨
                attribute = remove_enum_flags(attribute, RecordAttribute::kManual);
              }

              // options 파싱
              RecordAttributeOption option;
              option.duration_minutes = 0;

              if (hour_json.contains("options") && hour_json["options"].is_object()) {
                const auto& options_json = hour_json["options"];
                if (options_json.contains("duration_minutes") &&
                    options_json["duration_minutes"].is_number_unsigned()) {
                  option.duration_minutes =
                    options_json["duration_minutes"].get<uint32_t>();
                }
              }

              // HourlySchedule 설정
              auto& hourly =
                channel_schedule.weekday_schedules[weekday].hourly_schedules[hour];
              hourly.attribute = attribute;

              // 속성별로 옵션 저장
              if (attribute != RecordAttribute::kNone) {
                uint32_t attr_val = static_cast<uint32_t>(attribute);
                uint32_t mask = 0x00010000;

                for (int i = 0; i < 16; ++i, mask <<= 1) {
                  if ((attr_val & mask) != 0) {
                    hourly.options[i] = option;
                  }
                }
              }
            }
          }
        }
      }

      // 채널 스케줄 저장
      set_channel_schedule(channel_schedule);
    }

    return {};
  }
  catch (const json::exception&) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }
  catch (const std::exception&) {
    return std::unexpected(std::make_error_code(std::errc::io_error));
  }
}

void
RecordScheduler::set_channel_schedule(const ChannelSchedule& schedule)
{
  {
    std::unique_lock lock(m_mutex);
    m_channel_schedules[schedule.channel_id] = schedule;

    if (m_current_attributes.find(schedule.channel_id) == m_current_attributes.end()) {
      // mutex가 이미 잠긴 상태이므로 내부 함수 사용
      auto scheduled_attr = get_scheduled_attribute_internal(schedule.channel_id);
      m_current_attributes[schedule.channel_id] = scheduled_attr;
    }
  }
}

std::optional<ChannelSchedule>
RecordScheduler::get_channel_schedule(int64_t channel_id) const
{
  std::shared_lock lock(m_mutex);
  auto it = m_channel_schedules.find(channel_id);
  if (it != m_channel_schedules.end()) {
    return it->second;
  }
  return std::nullopt;
}

RecordAttribute
RecordScheduler::get_scheduled_attribute(int64_t channel_id) const
{
  std::shared_lock lock(m_mutex);
  return get_scheduled_attribute_internal(channel_id);
}

RecordAttribute
RecordScheduler::get_scheduled_attribute_internal(int64_t channel_id) const
{
  // mutex는 호출자가 이미 잠갔다고 가정
  auto it = m_channel_schedules.find(channel_id);
  if (it == m_channel_schedules.end()) {
    return RecordAttribute::kNone;
  }

  int hour = 0;
  int weekday = 0;
  extract_time_info(hour, weekday);

  const auto& daily_schedule = it->second.weekday_schedules[weekday];
  const auto& hourly_schedule = daily_schedule.hourly_schedules[hour];

  return hourly_schedule.attribute;
}

RecordAttribute
RecordScheduler::get_current_attribute(int64_t channel_id) const
{
  std::shared_lock lock(m_mutex);

  auto it = m_current_attributes.find(channel_id);
  if (it != m_current_attributes.end()) {
    return it->second;
  }

  return RecordAttribute::kNone;
}

nx::expected<AttributeChangeSuccess>
RecordScheduler::set_attribute(int64_t channel_id, RecordAttribute attribute)
{
  std::unique_lock lock(m_mutex);

  // Manual 플래그가 없는 경우, 스케줄 검증
  if ((attribute & RecordAttribute::kManual) == RecordAttribute::kNone) {
    // 채널 존재 여부 확인
    auto channel_it = m_channel_schedules.find(channel_id);
    if (channel_it == m_channel_schedules.end()) {
      return std::unexpected(make_error_code(ScheduleErrc::channel_not_found));
    }

    // 스케줄에 속성이 정의되어 있는지 확인
    RecordAttributeOption option;
    if (!is_attribute_in_schedule(channel_id, attribute, &option)) {
      return std::unexpected(make_error_code(ScheduleErrc::attribute_not_in_schedule));
    }

    // 현재 속성과 비교
    auto current_it = m_current_attributes.find(channel_id);
    RecordAttribute current_attr = (current_it != m_current_attributes.end())
                                     ? current_it->second
                                     : RecordAttribute::kNone;

    if (current_attr == attribute) {
      return std::unexpected(make_error_code(ScheduleErrc::attribute_already_set));
    }

    // 성공: 속성 변경
    set_current_attribute_internal(channel_id, attribute);
    return AttributeChangeSuccess{attribute, option};
  }
  else {
    // Manual 속성: 스케줄 검증 불필요
    auto current_it = m_current_attributes.find(channel_id);
    RecordAttribute current_attr = (current_it != m_current_attributes.end())
                                     ? current_it->second
                                     : RecordAttribute::kNone;

    if (current_attr == attribute) {
      return std::unexpected(make_error_code(ScheduleErrc::attribute_already_set));
    }

    set_current_attribute_internal(channel_id, attribute);
    return AttributeChangeSuccess{attribute, RecordAttributeOption{}};
  }
}

bool
RecordScheduler::check_time_changed()
{
  auto now = std::chrono::system_clock::now();

  {
    std::unique_lock lock(m_mutex);

    auto duration = now - m_last_checked_time;

    if (duration.count() < 0 || duration > nx::hours(1)) {
      m_last_checked_time = now;
      return true;
    }

    m_last_checked_time = now;
  }

  return false;
}

std::vector<int64_t>
RecordScheduler::on_hourly_schedule_change()
{
  std::vector<int64_t> changed_channels;

  {
    std::unique_lock lock(m_mutex);

    // 모든 채널에 대해 현재 시각의 스케줄 속성으로 복원
    for (auto& [channel_id, current_attr] : m_current_attributes) {
      // mutex가 이미 잠긴 상태이므로 내부 함수 사용
      auto scheduled_attr = get_scheduled_attribute_internal(channel_id);

      // Manual 비트 유지, 나머지는 스케줄로 복원
      RecordAttribute new_attr = set_enum_bit_or(
        scheduled_attr, set_enum_bit_and(current_attr, RecordAttribute::kManual));

      if (!enum_flags_equal(new_attr, current_attr)) {
        m_current_attributes[channel_id] = new_attr;
        changed_channels.push_back(channel_id);
      }
    }
  }

  return changed_channels;
}

void
RecordScheduler::set_current_attribute_internal(int64_t channel_id,
                                                RecordAttribute attribute)
{
  m_current_attributes[channel_id] = attribute;
}

void
RecordScheduler::extract_time_info(int& out_hour, int& out_weekday) const
{
  mstime_t current_ms = nx::now_ms();

  auto datetime_result = nx::util::timestamp_to_local_datetime(current_ms);
  if (!datetime_result.has_value()) {
    out_hour = 0;
    out_weekday = 0;
    return;
  }

  const auto& dt = datetime_result.value();

  out_hour = dt.tm.tm_hour;
  out_weekday = dt.tm.tm_wday;
}

bool
RecordScheduler::is_attribute_in_schedule(int64_t channel_id, RecordAttribute attribute,
                                          RecordAttributeOption* out_option) const
{
  auto it = m_channel_schedules.find(channel_id);
  if (it == m_channel_schedules.end()) {
    return false;
  }

  int hour = 0;
  int weekday = 0;
  extract_time_info(hour, weekday);

  const auto& daily_schedule = it->second.weekday_schedules[weekday];
  const auto& hourly_schedule = daily_schedule.hourly_schedules[hour];

  // 플래그 비트 AND 연산
  if (has_any_flag(static_cast<uint32_t>(hourly_schedule.attribute), attribute)) {
    if (out_option != nullptr) {
      uint32_t attr_val = static_cast<uint32_t>(attribute);
      uint32_t mask = 0x00010000;

      for (int i = 0; i < 16; ++i, mask <<= 1) {
        if ((attr_val & mask) != 0) {
          *out_option = hourly_schedule.options[i];
          break;
        }
      }
    }
    return true;
  }

  return false;
}

RecordAttribute
RecordScheduler::parse_attribute_string(const std::string& attr_str)
{
  RecordAttribute result = RecordAttribute::kNone;

  std::string remaining = attr_str;

  while (!remaining.empty()) {
    size_t pipe_pos = remaining.find('|');
    std::string single_attr;

    if (pipe_pos != std::string::npos) {
      single_attr = remaining.substr(0, pipe_pos);
      remaining = remaining.substr(pipe_pos + 1);
    }
    else {
      single_attr = remaining;
      remaining.clear();
    }

    single_attr.erase(0, single_attr.find_first_not_of(" \t"));
    single_attr.erase(single_attr.find_last_not_of(" \t") + 1);

    if (single_attr == "continuous") {
      result = add_enum_flags(result, RecordAttribute::kContinuous);
    }
    else if (single_attr == "motion") {
      result = add_enum_flags(result, RecordAttribute::kMotion);
    }
    else if (single_attr == "event") {
      result = add_enum_flags(result, RecordAttribute::kEvent);
    }
    else if (single_attr == "manual") {
      result = add_enum_flags(result, RecordAttribute::kManual);
    }
  }

  return result;
}

RecordAttributeOption
RecordScheduler::parse_attribute_option(uint32_t duration_minutes)
{
  RecordAttributeOption result;
  result.duration_minutes = duration_minutes;
  return result;
}

} // namespace record
} // namespace nx
