// 파일: record_scheduler_unittest.cpp
// 생성일: 2025-12-09
// 설명: RecordScheduler 유닛 테스트 (nlohmann/json)

#include <nxcore/record/record_scheduler.h>
#include <nxcore/util/time_util.h>

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <thread>


using namespace nx::record;
using nx::mstime_t;

namespace {

// 테스트 헬퍼: 현재 시간 정보 구하기 (time_util 사용)
struct CurrentTimeInfo
{
  int hour;    // 0-23
  int weekday; // 0=일요일, 1=월요일, ..., 6=토요일
};

CurrentTimeInfo
get_current_time_info()
{
  mstime_t current_ms = nx::now_ms();
  auto datetime_result = nx::util::timestamp_to_local_datetime(current_ms);

  if (!datetime_result.has_value()) {
    // 변환 실패 시 기본값
    return {0, 0};
  }

  const auto& dt = datetime_result.value();
  return {
    dt.tm.tm_hour, // 0~23
    dt.tm.tm_wday  // 0=일요일, 1=월요일, ..., 6=토요일
  };
}

// 테스트 헬퍼: 간단한 스케줄 생성
ChannelSchedule
create_test_schedule(int64_t channel_id, int weekday, int hour, RecordAttribute attribute)
{
  ChannelSchedule schedule;
  schedule.channel_id = channel_id;
  schedule.enabled = true;

  for (int d = 0; d < 7; ++d) {
    schedule.weekday_schedules[d].weekday = d;
    for (int h = 0; h < 24; ++h) {
      schedule.weekday_schedules[d].hourly_schedules[h].attribute =
        RecordAttribute::kNone;
    }
  }

  schedule.weekday_schedules[weekday].hourly_schedules[hour].attribute = attribute;

  return schedule;
}

// 테스트 JSON 파일 생성
std::string
create_test_json_file()
{
  std::string json_content = R"({
  "schedules": [
    {
      "channel_id": 1,
      "enabled": true,
      "weekdays": [
        {
          "weekday": 0,
          "hours": [
            { "hour": 8, "attribute": "continuous" },
            { "hour": 12, "attribute": "motion", "options": { "duration_minutes": 10 } },
            { "hour": 18, "attribute": "event", "options": { "duration_minutes": 5 } }
          ]
        },
        {
          "weekday": 1,
          "hours": [
            { "hour": 9, "attribute": "continuous" },
            { "hour": 17, "attribute": "motion|event", "options": { "duration_minutes": 15 } }
          ]
        },
        {
          "weekday": 2,
          "hours": [
            { "hour": 8, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 3,
          "hours": [
            { "hour": 8, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 4,
          "hours": [
            { "hour": 8, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 5,
          "hours": [
            { "hour": 8, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 6,
          "hours": [
            { "hour": 10, "attribute": "motion" }
          ]
        }
      ]
    },
    {
      "channel_id": 2,
      "enabled": true,
      "weekdays": [
        {
          "weekday": 0,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 1,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 2,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 3,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 4,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 5,
          "hours": [
            { "hour": 9, "attribute": "continuous" }
          ]
        },
        {
          "weekday": 6,
          "hours": [
            { "hour": 12, "attribute": "continuous" }
          ]
        }
      ]
    }
  ]
})";

  // 임시 디렉토리 생성
  std::filesystem::path temp_dir =
    std::filesystem::temp_directory_path() / "record_scheduler_test";
  std::filesystem::create_directories(temp_dir);

  // JSON 파일 생성
  std::string json_file = (temp_dir / "test_schedule.json").string();
  std::ofstream file(json_file);
  file << json_content;
  file.close();

  return json_file;
}

class RecordSchedulerTest : public ::testing::Test
{
protected:
  RecordScheduler scheduler;
};

} // namespace

// ===================================================================
// 기본 스케줄 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, SetAndGetSchedule)
{
  auto schedule = create_test_schedule(1, 0, 9, RecordAttribute::kContinuous);
  scheduler.set_channel_schedule(schedule);

  auto retrieved = scheduler.get_channel_schedule(1);
  ASSERT_TRUE(retrieved.has_value());
  EXPECT_EQ(retrieved->channel_id, 1);
  EXPECT_EQ(retrieved->weekday_schedules[0].hourly_schedules[9].attribute,
            RecordAttribute::kContinuous);
}

TEST_F(RecordSchedulerTest, GetNonExistentSchedule)
{
  auto retrieved = scheduler.get_channel_schedule(999);
  EXPECT_FALSE(retrieved.has_value());
}

// ===================================================================
// 속성 설정 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, SetManualAttributeWithoutSchedule)
{
  auto result = scheduler.set_attribute(1, RecordAttribute::kManual);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->new_attribute, RecordAttribute::kManual);
  EXPECT_EQ(scheduler.get_current_attribute(1), RecordAttribute::kManual);
}

TEST_F(RecordSchedulerTest, SetManualAttributeMultipleTimes)
{
  // 첫 번째 설정
  auto result1 = scheduler.set_attribute(1, RecordAttribute::kManual);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(result1->new_attribute, RecordAttribute::kManual);

  // 두 번째 설정 (동일 속성)
  auto result2 = scheduler.set_attribute(1, RecordAttribute::kManual);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), make_error_code(ScheduleErrc::attribute_already_set));
}

TEST_F(RecordSchedulerTest, SetAttributeNotInSchedule)
{
  // 채널이 존재하지 않는 경우
  auto result1 = scheduler.set_attribute(1, RecordAttribute::kContinuous);
  ASSERT_FALSE(result1.has_value());
  EXPECT_EQ(result1.error(), make_error_code(ScheduleErrc::channel_not_found));
  EXPECT_FALSE(is_retryable_schedule_error(result1.error()));

  // 채널은 존재하지만 현재 시간에 해당 속성이 스케줄에 없는 경우
  auto time_info = get_current_time_info();
  int current_hour = time_info.hour;
  int current_weekday = time_info.weekday;

  // 다른 시간대에만 스케줄 설정 (현재 시간이 아닌 다른 시간)
  int other_hour = (current_hour + 1) % 24;
  auto schedule =
    create_test_schedule(2, current_weekday, other_hour, RecordAttribute::kContinuous);
  scheduler.set_channel_schedule(schedule);

  // 현재 시간에는 속성이 없으므로 attribute_not_in_schedule 발생
  auto result2 = scheduler.set_attribute(2, RecordAttribute::kContinuous);
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), make_error_code(ScheduleErrc::attribute_not_in_schedule));

  // 재시도 가능한 오류인지 확인
  EXPECT_TRUE(is_retryable_schedule_error(result2.error()));
}

TEST_F(RecordSchedulerTest, SetAttributeForNonExistentChannel)
{
  auto result = scheduler.set_attribute(999, RecordAttribute::kContinuous);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(ScheduleErrc::channel_not_found));

  // 재시도 불가능한 오류인지 확인
  EXPECT_FALSE(is_retryable_schedule_error(result.error()));
}

TEST_F(RecordSchedulerTest, ErrorCategoryConsistency)
{
  // 에러 카테고리가 올바르게 설정되는지 확인
  auto result = scheduler.set_attribute(999, RecordAttribute::kContinuous);
  ASSERT_FALSE(result.has_value());

  const auto& error = result.error();
  EXPECT_EQ(error.category().name(), std::string("nx::record::schedule"));
  EXPECT_FALSE(error.message().empty());
}

// ===================================================================
// 속성 파싱 테스트
// ===================================================================

TEST(ParseAttributeStringTest, SingleAttribute)
{
  EXPECT_EQ(RecordScheduler::parse_attribute_string("continuous"),
            RecordAttribute::kContinuous);
  EXPECT_EQ(RecordScheduler::parse_attribute_string("motion"), RecordAttribute::kMotion);
  EXPECT_EQ(RecordScheduler::parse_attribute_string("event"), RecordAttribute::kEvent);
  EXPECT_EQ(RecordScheduler::parse_attribute_string("manual"), RecordAttribute::kManual);
  EXPECT_EQ(RecordScheduler::parse_attribute_string("none"), RecordAttribute::kNone);
}

TEST(ParseAttributeStringTest, MultipleAttributes)
{
  auto result = RecordScheduler::parse_attribute_string("motion|event");
  auto expected = RecordAttribute::kMotion | RecordAttribute::kEvent;
  EXPECT_EQ(result, expected);
}

TEST(ParseAttributeStringTest, WithWhitespace)
{
  EXPECT_EQ(RecordScheduler::parse_attribute_string("  continuous  "),
            RecordAttribute::kContinuous);
  auto result = RecordScheduler::parse_attribute_string("motion | event");
  auto expected = RecordAttribute::kMotion | RecordAttribute::kEvent;
  EXPECT_EQ(result, expected);
}

// ===================================================================
// 다중 채널 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, MultiChannelManagement)
{
  // time_util 사용하여 현재 시간 정보 구하기
  auto time_info = get_current_time_info();
  int current_hour = time_info.hour;
  int current_weekday = time_info.weekday;

  for (int ch = 1; ch <= 5; ++ch) {
    auto schedule =
      create_test_schedule(ch, current_weekday, current_hour,
                           static_cast<RecordAttribute>(0x00010000 << (ch - 1)));
    scheduler.set_channel_schedule(schedule);
  }

  for (int ch = 1; ch <= 5; ++ch) {
    auto attr = scheduler.get_current_attribute(ch);
    EXPECT_EQ(attr, static_cast<RecordAttribute>(0x00010000 << (ch - 1)));
  }
}

// ===================================================================
// JSON 파일 로드 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, LoadScheduleFromJsonFile)
{
  // 테스트 JSON 파일 생성
  std::string json_file = create_test_json_file();

  // JSON 파일 로드
  auto result = scheduler.load_schedule_from_file(json_file);
  ASSERT_TRUE(result.has_value());

  // 채널 1 검증
  auto ch1_schedule = scheduler.get_channel_schedule(1);
  ASSERT_TRUE(ch1_schedule.has_value());
  EXPECT_EQ(ch1_schedule->channel_id, 1);
  EXPECT_TRUE(ch1_schedule->enabled);

  // 요일 0, 시간 8: continuous
  EXPECT_EQ(ch1_schedule->weekday_schedules[0].hourly_schedules[8].attribute,
            RecordAttribute::kContinuous);

  // 요일 0, 시간 12: motion
  EXPECT_EQ(ch1_schedule->weekday_schedules[0].hourly_schedules[12].attribute,
            RecordAttribute::kMotion);
  EXPECT_EQ(
    ch1_schedule->weekday_schedules[0].hourly_schedules[12].options[1].duration_minutes,
    10);

  // 요일 1, 시간 17: motion|event
  auto attr_17 = ch1_schedule->weekday_schedules[1].hourly_schedules[17].attribute;
  EXPECT_NE(attr_17 & RecordAttribute::kMotion, RecordAttribute::kNone);
  EXPECT_NE(attr_17 & RecordAttribute::kEvent, RecordAttribute::kNone);

  // 채널 2 검증
  auto ch2_schedule = scheduler.get_channel_schedule(2);
  ASSERT_TRUE(ch2_schedule.has_value());
  EXPECT_EQ(ch2_schedule->channel_id, 2);

  // 정리
  std::filesystem::remove(json_file);
}

TEST_F(RecordSchedulerTest, LoadScheduleFromFileNotFound)
{
  auto result = scheduler.load_schedule_from_file("/nonexistent/path/schedule.json");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), std::make_error_code(std::errc::no_such_file_or_directory));
}

TEST_F(RecordSchedulerTest, LoadScheduleFromInvalidJson)
{
  // 임시 디렉토리 생성
  std::filesystem::path temp_dir =
    std::filesystem::temp_directory_path() / "record_scheduler_test";
  std::filesystem::create_directories(temp_dir);

  // 유효하지 않은 JSON 파일 생성
  std::string json_file = (temp_dir / "invalid.json").string();
  std::ofstream file(json_file);
  file << "{ invalid json }";
  file.close();

  // 로드 시도
  auto result = scheduler.load_schedule_from_file(json_file);
  EXPECT_FALSE(result.has_value());

  // 정리
  std::filesystem::remove(json_file);
}

TEST_F(RecordSchedulerTest, LoadScheduleWithMissingSchedulesField)
{
  // 임시 디렉토리 생성
  std::filesystem::path temp_dir =
    std::filesystem::temp_directory_path() / "record_scheduler_test";
  std::filesystem::create_directories(temp_dir);

  // "schedules" 필드 없는 JSON 파일
  std::string json_file = (temp_dir / "no_schedules.json").string();
  std::ofstream file(json_file);
  file << "{ \"data\": [] }";
  file.close();

  // 로드 시도
  auto result = scheduler.load_schedule_from_file(json_file);
  EXPECT_FALSE(result.has_value());

  // 정리
  std::filesystem::remove(json_file);
}

// ===================================================================
// JSON 로드 후 속성 조회 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, GetScheduledAttributeAfterJsonLoad)
{
  // JSON 파일 생성 및 로드
  std::string json_file = create_test_json_file();
  auto load_result = scheduler.load_schedule_from_file(json_file);
  ASSERT_TRUE(load_result.has_value());

  // 채널 1, 요일 0, 시간 8: continuous 속성 확인
  auto ch1_schedule = scheduler.get_channel_schedule(1);
  ASSERT_TRUE(ch1_schedule.has_value());

  // 시간 8의 속성 (continuous)
  auto attr_8 = ch1_schedule->weekday_schedules[0].hourly_schedules[8].attribute;
  EXPECT_EQ(attr_8, RecordAttribute::kContinuous);

  // 정리
  std::filesystem::remove(json_file);
}

// ===================================================================
// Manual 속성 필터링 테스트
// ===================================================================

TEST_F(RecordSchedulerTest, ManualAttributeIgnoredInJsonLoad)
{
  // Manual 속성을 포함한 임시 JSON 파일 생성
  std::filesystem::path temp_dir =
    std::filesystem::temp_directory_path() / "record_scheduler_test";
  std::filesystem::create_directories(temp_dir);

  std::string json_content = R"({
  "schedules": [
    {
      "channel_id": 1,
      "enabled": true,
      "weekdays": [
        {
          "weekday": 0,
          "hours": [
            { "hour": 10, "attribute": "manual", "options": { "duration_minutes": 0 } },
            { "hour": 11, "attribute": "continuous|manual" }
          ]
        },
        {
          "weekday": 1,
          "hours": []
        },
        {
          "weekday": 2,
          "hours": []
        },
        {
          "weekday": 3,
          "hours": []
        },
        {
          "weekday": 4,
          "hours": []
        },
        {
          "weekday": 5,
          "hours": []
        },
        {
          "weekday": 6,
          "hours": []
        }
      ]
    }
  ]
})";

  std::string json_file = (temp_dir / "manual_test.json").string();
  std::ofstream file(json_file);
  file << json_content;
  file.close();

  // JSON 로드
  auto result = scheduler.load_schedule_from_file(json_file);
  ASSERT_TRUE(result.has_value());

  // 검증: hour 10의 attribute는 kNone이어야 함 (manual 필터링됨)
  auto ch1_schedule = scheduler.get_channel_schedule(1);
  ASSERT_TRUE(ch1_schedule.has_value());

  auto attr_10 = ch1_schedule->weekday_schedules[0].hourly_schedules[10].attribute;
  EXPECT_EQ(attr_10, RecordAttribute::kNone);
  EXPECT_EQ(attr_10 & RecordAttribute::kManual, RecordAttribute::kNone);

  // 검증: hour 11의 attribute는 continuous만 남아야 함 (manual 필터링됨)
  auto attr_11 = ch1_schedule->weekday_schedules[0].hourly_schedules[11].attribute;
  EXPECT_EQ(attr_11, RecordAttribute::kContinuous);
  EXPECT_EQ(attr_11 & RecordAttribute::kManual, RecordAttribute::kNone);

  // 정리
  std::filesystem::remove(json_file);
}

TEST_F(RecordSchedulerTest, ManualAttributeCanBeSetAtRuntime)
{
  // time_util 사용하여 현재 시간 정보 구하기
  auto time_info = get_current_time_info();
  int current_hour = time_info.hour;
  int current_weekday = time_info.weekday;

  // 현재 시간에 해당하는 스케줄 설정
  auto schedule =
    create_test_schedule(1, current_weekday, current_hour, RecordAttribute::kContinuous);
  scheduler.set_channel_schedule(schedule);

  // 현재 속성: kContinuous (현재 시간의 스케줄)
  auto attr_before = scheduler.get_current_attribute(1);
  EXPECT_EQ(attr_before, RecordAttribute::kContinuous);

  // 실시간 실행 중 Manual 속성 추가
  auto result = scheduler.set_attribute(1, RecordAttribute::kManual);
  ASSERT_TRUE(result.has_value());

  // 현재 속성: kManual (Manual은 사용자가 실시간으로 설정)
  auto attr_after = scheduler.get_current_attribute(1);
  EXPECT_EQ(attr_after, RecordAttribute::kManual);
}

TEST_F(RecordSchedulerTest, ManualAttributePreservedInHourlyChange)
{
  // JSON 파일 생성 및 로드
  std::string json_file = create_test_json_file();
  auto load_result = scheduler.load_schedule_from_file(json_file);
  ASSERT_TRUE(load_result.has_value());

  // 현재 속성: 스케줄된 속성 (시간에 따라 다름)
  // 실시간 실행 중 Manual 속성 추가
  auto set_result = scheduler.set_attribute(1, RecordAttribute::kManual);
  ASSERT_TRUE(set_result.has_value());

  // 정각 갱신 실행
  auto changed_channels = scheduler.on_hourly_schedule_change();

  // Manual 속성은 보존되어야 함
  auto attr_after = scheduler.get_current_attribute(1);
  EXPECT_NE(attr_after & RecordAttribute::kManual, RecordAttribute::kNone);

  // 정리
  std::filesystem::remove(json_file);
}
