#pragma once

#include "schedule.h"

#include <nxcore/util/time_util.h>

#include <cstdint>
#include <expected>
#include <map>
#include <memory>
#include <optional>
#include <shared_mutex>
#include <string>
#include <system_error>
#include <vector>

namespace nx {
namespace record {

// ===================================================================
// 레코드 스케줄러 (메인 클래스)
// ===================================================================

class RecordScheduler
{
public:
  explicit RecordScheduler();
  ~RecordScheduler();

  // 복사 및 이동 금지
  RecordScheduler(const RecordScheduler&) = delete;
  RecordScheduler& operator=(const RecordScheduler&) = delete;

  // ===== 스케줄 설정 =====

  // JSON 파일로부터 모든 채널의 스케줄 로드 (nlohmann/json 사용)
  nx::expected<void> load_schedule_from_file(const std::string& schedule_path);

  // 특정 채널의 스케줄 설정
  void set_channel_schedule(const ChannelSchedule& schedule);

  // 특정 채널의 스케줄 조회
  std::optional<ChannelSchedule> get_channel_schedule(int64_t channel_id) const;

  // ===== 현재 속성 조회 =====

  // 현재 시각 기준, 채널의 스케줄된 속성 조회
  // (수동 녹화로 인한 현재 활성 속성은 별도)
  // @param channel_id: 채널 ID
  // @return: 현재 로컬 시간에 해당하는 스케줄 속성, 스케줄이 없으면 kNone
  RecordAttribute get_scheduled_attribute(int64_t channel_id) const;

  // 수동 녹화를 포함하여 현재 실제 활성 속성 조회
  // @param channel_id: 채널 ID
  // @return: 현재 활성 속성 (스케줄 속성과 수동 속성의 합성)
  RecordAttribute get_current_attribute(int64_t channel_id) const;

  // ===== 속성 변경 (외부 호출용) =====

  // 속성 설정 (스케줄 기반 또는 수동)
  // - Manual이 아닌 경우: 현재 시각의 스케줄에서 해당 속성이 정의되어 있어야함
  // - 동일 속성이면 ScheduleErrc::attribute_already_set 반환
  // - 변경 성공 시 새 속성과 옵션 반환
  // @param channel_id: 채널 ID
  // @param attribute: 설정할 속성
  // @return: 성공 시 AttributeChangeSuccess, 실패 시 std::error_code
  nx::expected<AttributeChangeSuccess> set_attribute(int64_t channel_id,
                                                     RecordAttribute attribute);

  // ===== 시간 변경 감지 =====

  // 외부에서 주기적으로 호출 (매 1초, 또는 주요 이벤트 이후)
  // 시간이 변경되었으면 true 반환
  // 시간이 역행했거나 1시간 이상 점프한 경우 시간 변경으로 판단
  // @return: 시간이 변경되었으면 true
  bool check_time_changed();

  // ===== 정각 갱신 (1시간마다 호출) =====

  // 1시간마다 호출: 현재 시각의 스케줄된 속성으로 자동 복원
  // (수동 녹화로 인한 변경은 취소, 스케줄 속성으로 돌아감)
  // 변경된 채널 목록 반환
  // @return: 정각 갱신으로 인해 속성이 변경된 채널 ID 목록
  std::vector<int64_t> on_hourly_schedule_change();

  // ===== JSON 파싱 헬퍼 함수 (public, 테스트용) =====

  // 문자열로부터 RecordAttribute enum 파싱
  // @param attr_str: "continuous", "motion", "event", "manual", "none" 등
  // @return: 파싱된 RecordAttribute enum 값
  static RecordAttribute parse_attribute_string(const std::string& attr_str);

  // 옵션 객체 파싱
  // @param duration_minutes: 녹화 지속 시간 (분, 0=무한)
  // @return: 파싱된 RecordAttributeOption
  static RecordAttributeOption parse_attribute_option(uint32_t duration_minutes);

private:
  // 채널의 현재 실제 속성 설정 (내부용)
  void set_current_attribute_internal(int64_t channel_id, RecordAttribute attribute);

  // 현재 시각(로컬 타임 기준)에서 시간(hour)과 요일(weekday) 추출
  // @param out_hour: 추출된 시간 (0~23)
  // @param out_weekday: 추출된 요일 (0=일요일, ..., 6=토요일)
  void extract_time_info(int& out_hour, int& out_weekday) const;

  // 속성이 스케줄에 정의되어 있는지 확인
  // @param channel_id: 채널 ID
  // @param attribute: 확인할 속성
  // @param out_option: 속성의 옵션 정보 (있을 경우)
  // @return: 속성이 정의되어 있으면 true
  bool is_attribute_in_schedule(int64_t channel_id, RecordAttribute attribute,
                                RecordAttributeOption* out_option = nullptr) const;

  // 스케줄된 속성 조회 (내부용, mutex 없음)
  // @param channel_id: 채널 ID
  // @return: 현재 로컬 시간에 해당하는 스케줄 속성
  RecordAttribute get_scheduled_attribute_internal(int64_t channel_id) const;

  // 마지막 감지한 시간
  mutable std::shared_mutex m_mutex;
  std::chrono::system_clock::time_point m_last_checked_time;

  // 채널별 스케줄
  std::map<int64_t, ChannelSchedule> m_channel_schedules;

  // 채널별 현재 실제 속성
  std::map<int64_t, RecordAttribute> m_current_attributes;
};

} // namespace record
} // namespace nx
