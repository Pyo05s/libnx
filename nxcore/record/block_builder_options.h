// 파일: block_builder_options.h
// 생성일: 2026-01-14
// 설명: BlockBuilder Options 기본 구현체

#pragma once

#include "block_builder.h"
#include <nxcore/util/time_util.h>
#include <cstddef>

namespace nx {
namespace record {

// ========================================================================
// BasicBlockBuilderOptions: BlockBuilder::Options 기본 구현
// ========================================================================
//
// 블록 완료 조건에 대한 표준 구현을 제공합니다.
// - 시간 기반: max_duration 경과 시 완료
// - 크기 기반: pending_bytes가 max_size 이상 시 완료
// - 카운트 기반: pending_count가 max_entry_count 이상 시 완료
//
// 확장 포인트:
// - should_finish_on_duration(): 시간 조건 만족 시 추가 검사
// - should_finish_on_size(): 크기 조건 만족 시 추가 검사
// - should_finish_on_count(): 카운트 조건 만족 시 추가 검사
//
// 사용 예:
//   auto options = std::make_shared<BasicBlockBuilderOptions>(
//       nx::seconds(10),  // 최대 10초
//       4 * 1024 * 1024,           // 최대 4MB
//       1000                       // 최대 1000개 엔트리
//   );
//   BlockBuilder builder(options);
//
class BasicBlockBuilderOptions : public BlockBuilder::Options
{
public:
  using Context = BlockBuilder::Context;

  // 생성자
  explicit BasicBlockBuilderOptions(
    nx::milliseconds max_duration = nx::seconds(10),
    std::size_t max_size_bytes = 4 * 1024 * 1024,
    std::size_t max_entry_count = 1000)
      : m_max_duration(max_duration)
      , m_max_size_bytes(max_size_bytes)
      , m_max_entry_count(max_entry_count)
      , m_reject_past_entries(true) // 기본값: true
  {}

  ~BasicBlockBuilderOptions() override = default;

  // ========================================================================
  // BlockBuilder::Options 인터페이스 구현
  // ========================================================================

  bool is_finished(Context& ctx) const override
  {
    // 엔트리가 없으면 완료하지 않음
    if (ctx.pending_count == 0) {
      return false;
    }

    // 각 완료 조건 검사 (확장 포인트 호출)
    if (should_finish_on_duration(ctx)) {
      return true;
    }

    if (should_finish_on_size(ctx)) {
      return true;
    }

    if (should_finish_on_count(ctx)) {
      return true;
    }

    return false;
  }

  bool should_finalize_before_add(
    const Context& ctx, mstime_t, mstime_t new_entry_end_timestamp) const override
  {
    // pending entries가 없으면 체크 불필요 (호출되지 않아야 하지만 안전 체크)
    if (ctx.pending_count == 0) {
      return false;
    }

    // 새 엔트리를 추가했을 때 블록의 duration이 max_duration을 초과하는지 확인
    mstime_t would_be_duration = new_entry_end_timestamp - ctx.start_timestamp;

    // max_duration 이상이면 현재 블록을 완료하고 새 블록에 추가
    return would_be_duration >= m_max_duration.count();
  }

  bool
  should_accept_entry(const Context& ctx, mstime_t new_entry_timestamp) const override
  {
    if (!m_reject_past_entries || ctx.pending_count == 0) {
      return true; // 비활성화 상태이거나 첫 엔트리는 항상 허용
    }

    // 현재 블록 시작 시간보다 이전 엔트리 거부
    return new_entry_timestamp >= ctx.start_timestamp;
  }

  void on_entry_rejected(mstime_t, mstime_t) const override
  {
    // TODO: 필요시 로깅 추가
    // 예: NX_WARN("Entry rejected: ts={}, block_start={}",
    //            rejected_timestamp, current_start_timestamp);
  }

  nx::milliseconds get_max_duration() const override { return m_max_duration; }

  std::size_t get_max_size() const override { return m_max_size_bytes; }

  std::size_t get_max_entry_count() const override { return m_max_entry_count; }

  // Timeout 체크 (기본 구현: max_duration * 2 경과 시)
  bool is_timed_out(const Context& ctx) const override
  {
    auto now = std::chrono::steady_clock::now();
    auto elapsed
      = std::chrono::duration_cast<nx::milliseconds>(now - ctx.last_activity_time);
    return elapsed >= (m_max_duration * 2);
  }

  // ========================================================================
  // 설정 메서드
  // ========================================================================

  void set_max_duration(nx::milliseconds duration) { m_max_duration = duration; }

  void set_max_size(std::size_t bytes) { m_max_size_bytes = bytes; }

  void set_max_entry_count(std::size_t count) { m_max_entry_count = count; }

  // Timestamp 검증 활성화/비활성화
  // enable=true: 블록 시작 시간보다 이전 엔트리 거부
  // enable=false: 모든 엔트리 허용 (기본 동작)
  void enable_timestamp_validation(bool enable = true) { m_reject_past_entries = enable; }

  bool is_timestamp_validation_enabled() const { return m_reject_past_entries; }

protected:
  // ========================================================================
  // 확장 포인트: 파생 클래스에서 추가 조건 구현 가능
  // ========================================================================

  // 시간 기반 완료 조건 검사
  // 반환값: true = 블록 완료, false = 계속 대기
  virtual bool should_finish_on_duration(Context& ctx) const
  {
    if (ctx.start_timestamp >= 0 && ctx.end_timestamp >= 0) {
      mstime_t duration_ms = ctx.end_timestamp - ctx.start_timestamp;
      mstime_t max_duration_ms = m_max_duration.count();
      return duration_ms >= max_duration_ms;
    }
    return false;
  }

  // 크기 기반 완료 조건 검사
  // 반환값: true = 블록 완료, false = 계속 대기
  virtual bool should_finish_on_size(Context& ctx) const
  {
    return ctx.pending_bytes >= m_max_size_bytes;
  }

  // 카운트 기반 완료 조건 검사
  // 반환값: true = 블록 완료, false = 계속 대기
  virtual bool should_finish_on_count(Context& ctx) const
  {
    return ctx.pending_count >= m_max_entry_count;
  }

private:
  nx::milliseconds m_max_duration;
  std::size_t m_max_size_bytes;
  std::size_t m_max_entry_count;
  bool m_reject_past_entries; // timestamp 검증 활성화 여부
};

} // namespace record
} // namespace nx
