// 파일: schedule.h
// 생성일: 2025-12-09
// 설명: 녹화 스케줄 관련 타입 정의 (요일/시간/속성 그리드)

#pragma once

#include <array>
#include <cstdint>
#include <string>
#include <system_error>

#include "../util/enum_util.h"

namespace nx {
namespace record {

// ===================================================================
// 녹화 속성(Type) 정의
// ===================================================================
// BlockHeader.flags의 상위 16비트(0x00010000 ~ 0x10000000)에 기록
// uint32_t로 표현하되, 여러 속성을 조합 가능 (flag)

enum class RecordAttribute : uint32_t
{
  kNone = 0x00000000,
  kContinuous = 0x00010000, // 상시 녹화
  kMotion = 0x00020000,     // 모션 감지 녹화
  kEvent = 0x00040000,      // 이벤트 기반 녹화
  kManual = 0x00080000,     // 수동 녹화
                            // 추가 속성: 0x00100000 ~ 0x10000000

  kRecordMask = 0x000F0000, // 녹화 속성 마스크
};

// RecordAttribute 비트 연산자 정의
// enum_util.h의 함수들과 함께 사용 가능
inline constexpr RecordAttribute
operator|(RecordAttribute a, RecordAttribute b) noexcept
{
  return static_cast<RecordAttribute>(
    static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
}

inline constexpr RecordAttribute
operator&(RecordAttribute a, RecordAttribute b) noexcept
{
  return static_cast<RecordAttribute>(
    static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
}

inline constexpr RecordAttribute
operator^(RecordAttribute a, RecordAttribute b) noexcept
{
  return static_cast<RecordAttribute>(
    static_cast<uint32_t>(a) ^ static_cast<uint32_t>(b));
}

inline constexpr RecordAttribute
operator~(RecordAttribute a) noexcept
{
  return static_cast<RecordAttribute>(~static_cast<uint32_t>(a));
}

inline constexpr RecordAttribute&
operator|=(RecordAttribute& a, RecordAttribute b) noexcept
{
  a = a | b;
  return a;
}

inline constexpr RecordAttribute&
operator&=(RecordAttribute& a, RecordAttribute b) noexcept
{
  a = a & b;
  return a;
}

inline constexpr RecordAttribute&
operator^=(RecordAttribute& a, RecordAttribute b) noexcept
{
  a = a ^ b;
  return a;
}

// ===================================================================
// 속성별 추가 옵션
// ===================================================================

struct RecordAttributeOption
{
  // duration_minutes: 0 = 무한(수동 중지), > 0 = x분간 녹화
  uint32_t duration_minutes = 0;

  // 향후 확장용 예약 필드
  uint32_t reserved[3] = {0};
};

// ===================================================================
// 시간별 속성 설정 (특정 시간에 어떤 속성이 활성인지)
// ===================================================================

struct HourlySchedule
{
  // 이 시간에 활성인 속성들 (flag)
  RecordAttribute attribute = RecordAttribute::kNone;

  // 속성별 옵션 (각 속성에 대한 추가 정보)
  // 인덱스: 0x00010000 >> 16 = 1 (Continuous)
  //         0x00020000 >> 16 = 2 (Motion)
  //         0x00040000 >> 16 = 3 (Event)
  //         0x00080000 >> 16 = 4 (Manual)
  RecordAttributeOption options[16];

  // 예약 필드
  uint8_t reserved[16] = {0};
};

// ===================================================================
// 요일별 스케줄 (특정 요일의 0~23시 스케줄)
// ===================================================================

struct DailySchedule
{
  // 요일: 0=일요일, 1=월요일, ..., 6=토요일
  int weekday = 0;

  // 24시간 스케줄 (hourly_schedules[0] = 0시, hourly_schedules[23] = 23시)
  std::array<HourlySchedule, 24> hourly_schedules;
};

// ===================================================================
// 채널별 주간 스케줄 (한 주의 완전한 스케줄)
// ===================================================================

struct ChannelSchedule
{
  int64_t channel_id = 0;

  // 7일간의 스케줄 (weekday_schedules[0] = 일요일, ..., [6] = 토요일)
  std::array<DailySchedule, 7> weekday_schedules;

  // 이 스케줄이 유효한지 여부
  bool enabled = true;
};

// ===================================================================
// 스케줄 오류 코드
// ===================================================================

enum class ScheduleErrc
{
  // 성공
  success = 0,

  // 속성 변경 오류
  attribute_not_in_schedule = 1, // 현재 시간의 스케줄에 해당 속성 없음
  attribute_already_set = 2,     // 이미 동일한 속성으로 설정됨
  channel_not_found = 3,         // 채널이 존재하지 않음
  invalid_attribute = 4,         // 유효하지 않은 속성값
  schedule_not_loaded = 5,       // 스케줄이 로드되지 않음

  // 스케줄 로드 오류
  schedule_parse_error = 10,      // 스케줄 파싱 오류
  schedule_validation_error = 11, // 스케줄 검증 오류
  invalid_weekday = 12,           // 유효하지 않은 요일 (0-6 범위 벗어남)
  invalid_hour = 13,              // 유효하지 않은 시간 (0-23 범위 벗어남)
};

// 스케줄 오류 코드 카테고리
class ScheduleErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::record::schedule"; }

  std::string message(int ev) const override
  {
    switch (static_cast<ScheduleErrc>(ev)) {
    case ScheduleErrc::success:
      return "Success";

    // 속성 변경 오류
    case ScheduleErrc::attribute_not_in_schedule:
      return "Attribute not defined in current schedule";
    case ScheduleErrc::attribute_already_set:
      return "Attribute already set";
    case ScheduleErrc::channel_not_found:
      return "Channel not found";
    case ScheduleErrc::invalid_attribute:
      return "Invalid attribute value";
    case ScheduleErrc::schedule_not_loaded:
      return "Schedule not loaded";

    // 스케줄 로드 오류
    case ScheduleErrc::schedule_parse_error:
      return "Failed to parse schedule";
    case ScheduleErrc::schedule_validation_error:
      return "Schedule validation failed";
    case ScheduleErrc::invalid_weekday:
      return "Invalid weekday value (must be 0-6)";
    case ScheduleErrc::invalid_hour:
      return "Invalid hour value (must be 0-23)";

    default:
      return "Unknown schedule error";
    }
  }

  // 오류가 재시도 가능한지 판단
  bool is_retryable(int ev) const noexcept
  {
    switch (static_cast<ScheduleErrc>(ev)) {
    // 재시도 불가능한 영구적 오류
    case ScheduleErrc::attribute_already_set:
    case ScheduleErrc::channel_not_found:
    case ScheduleErrc::invalid_attribute:
    case ScheduleErrc::schedule_parse_error:
    case ScheduleErrc::schedule_validation_error:
    case ScheduleErrc::invalid_weekday:
    case ScheduleErrc::invalid_hour:
      return false;

    // 재시도 가능한 오류 (시간이 바뀌면 성공할 수 있음)
    case ScheduleErrc::attribute_not_in_schedule:
    case ScheduleErrc::schedule_not_loaded:
      return true;

    default:
      return false;
    }
  }
};

// 싱글톤 카테고리 인스턴스
inline const ScheduleErrorCategory&
schedule_error_category()
{
  static ScheduleErrorCategory instance;
  return instance;
}

// error_code 생성 헬퍼
inline std::error_code
make_error_code(ScheduleErrc e)
{
  return {static_cast<int>(e), schedule_error_category()};
}

// is_retryable 헬퍼 함수
inline bool
is_retryable_schedule_error(const std::error_code& ec)
{
  if (ec.category() == schedule_error_category()) {
    return static_cast<const ScheduleErrorCategory&>(ec.category())
      .is_retryable(ec.value());
  }
  return false;
}

// ===================================================================
// 속성 변경 성공 결과
// ===================================================================

struct AttributeChangeSuccess
{
  RecordAttribute new_attribute = RecordAttribute::kNone;
  RecordAttributeOption option;
};

} // namespace record
} // namespace nx

// std::error_code와 통합
namespace std {
template <>
struct is_error_code_enum<nx::record::ScheduleErrc> : true_type
{
};
} // namespace std
