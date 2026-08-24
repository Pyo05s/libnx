// block_builder.h
// 2025-11-24
// 설명: 녹화 파일의 블록과 엔트리를 생성하는 빌더 클래스 구현

#pragma once

#include "block_buffer_pool.h"
#include "record.h"

#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <vector>
#include <memory>
#include <atomic>

namespace nx {
namespace record {

// forward declaration for BlockEntryBuffer defined in record.h
struct BlockEntryBuffer;

class BlockBuilder
{
public:
  // 현재 블록과 관련된 컨텍스트 정보
  class Context
  {
  public:
    Context() = default;

    void clear()
    {
      start_timestamp = 0;
      end_timestamp = 0;
      pending_count = 0;
      pending_bytes = 0;
      contains_keyframe = false;
      // last_activity_time은 블록이 완료되어도 유지됨 (timeout 체크용)
    }

  public:
    mstime_t start_timestamp = 0;   // 블록 시작 타임스탬프 (밀리초)
    mstime_t end_timestamp = 0;     // 블록 종료 타임스탬프 (밀리초)
    std::size_t pending_count = 0;  // 현재 대기중인 엔트리 수
    std::size_t pending_bytes = 0;  // 현재 대기중인 엔트리들의 총 바이트(헤더+페이로드)
    bool contains_keyframe = false; // I-프레임(키프레임)이 포함되었는지 여부
    std::chrono::steady_clock::time_point
      last_activity_time; // 마지막 활동 시간 (timeout 체크용)
  };

  // 옵션 인터페이스: 블록 완료 여부를 외부에서 결정하도록 위임합니다.
  class Options
  {
  public:
    virtual ~Options() = default;

  public:
    // 블록 완료 여부 판단 시 빌더 컨텍스트를 전달합니다.
    virtual bool is_finished(Context& ctx) const = 0;

    // 새 엔트리 추가 전 현재 블록을 완료해야 하는지 판단
    // 반환값: true = 현재 블록 완료 후 새 블록에 추가, false = 현재 블록에 추가
    // 참고: 이 메서드는 pending_count > 0 일 때만 호출됩니다.
    virtual bool should_finalize_before_add(const Context&, mstime_t, mstime_t) const
    {
      return false; // 기본: 체크 안 함
    }

    // 엔트리 추가 가능 여부 판단 (timestamp 검증 등)
    // 반환값: true = 추가 가능, false = 거부
    // 참고: 첫 엔트리(pending_count == 0)는 체크하지 않고 무조건 허용됩니다.
    virtual bool should_accept_entry(const Context&, mstime_t) const
    {
      return true; // 기본: 모든 엔트리 허용
    }

    // 엔트리 거부 시 호출되는 콜백 (기본: 아무것도 하지 않음)
    // 파생 클래스에서 로깅 등의 처리 구현 가능
    virtual void on_entry_rejected(mstime_t, mstime_t) const
    {
      // 기본: 아무것도 하지 않음
    }

    // Timeout 여부 판단 (기본: timeout 없음)
    // 반환값: true = timeout 발생, false = timeout 아님
    virtual bool is_timed_out(const Context&) const { return false; }

    // Timeout 발생 알림 콜백 (기본: 아무것도 하지 않음)
    virtual void on_timeout_flush() {}

    // 설정 조회 메서드 (인터페이스)
    virtual nx::milliseconds get_max_duration() const = 0;
    virtual std::size_t get_max_size() const = 0;
    virtual std::size_t get_max_entry_count() const = 0;
  };

public:
  // shared_ptr로 Options를 받아 생명주기 안전성 보장
  // pool_max_free: 블록 버퍼 풀의 최대 보관 수 (PendingBlockBudget의 min_pending에
  // 맞춰 설정)
  explicit BlockBuilder(
    std::shared_ptr<Options> options,
    std::size_t pool_max_free = BlockBufferPool::kDefaultMaxFreeBuffers);
  ~BlockBuilder();

  // 복사 및 이동 생성자/대입 연산자 삭제
  NX_NON_COPYABLE_AND_MOVABLE(BlockBuilder);

public:
  // 이제 add_entry는 BlockEntryBuffer를 소유하는 shared_ptr을 받습니다.
  void add_entry(std::shared_ptr<BlockEntryBuffer> entry_buf);

  // 남은 pending entries를 강제로 블록으로 완성
  void flush() { finalize_current_block(); }

  // block 구조
  // | 데이터 블록 헤더 | 데이터 블록 엔트리 1 | ... | 데이터 블록 엔트리 N |
  // 0xFFFB |
  DataBlock pop_block();
  std::size_t get_block_count() const;

  // Options 접근자 (timeout 체크용)
  std::shared_ptr<Options> get_options() const { return m_options; }

  // Context 조회 (블록 빌더의 현재 상태 정보)
  const Context& get_context() const { return m_ctx; }

  // 활동 시간 리셋 (BlockBuilder 재사용 시)
  void reset_activity_time()
  {
    m_ctx.last_activity_time = std::chrono::steady_clock::now();
  }

private:
  // 현재 대기 중인 엔트리들을 직렬화하여 DataBlock 으로 변환 후 m_blocks 에 추가
  void finalize_current_block();

  // 엔트리의 종료 timestamp 계산 (audio의 경우 duration 포함)
  mstime_t calculate_entry_end_timestamp(const BlockEntryBuffer* entry_buf) const;

private:
  std::shared_ptr<Options> m_options;      // shared_ptr로 변경
  std::shared_ptr<BlockBufferPool> m_pool; // 블록 직렬화 버퍼 메모리 풀

  std::vector<DataBlock> m_blocks;

  // 현재 블록에 포함될 대기 엔트리 목록 (직렬화/정렬 전)
  std::vector<std::shared_ptr<BlockEntryBuffer>> m_pending_entries;

  Context m_ctx; // 빌더 컨텍스트 (타임스탬프, 카운트, 활동 시간 등)
};

} // namespace record
} // namespace nx