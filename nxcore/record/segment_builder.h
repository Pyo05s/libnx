// 파일: segment_builder.h
// 생성일: 2025-12-01
// 설명: 세그먼트 빌더 클래스

#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>


#include "../file/file_stream.h"

#include "../util/time_util.h"
#include "../util/type_util.h"
#include "record.h"


namespace fs = std::filesystem;

namespace nx {
namespace record {

// SegmentBuilder는 세그먼트 파일 생성 및 관리를 담당합니다.
// BlockBuilder로부터 완성된 블록을 받아 파일에 기록하는 역할을 수행합니다.
class SegmentBuilder
{
public:
  // 현재 세그먼트와 관련된 컨텍스트 정보
  class Context
  {
  public:
    Context() = default;

    void clear()
    {
      start_timestamp = 0;
      end_timestamp = 0;
      block_count = 0;
      file_size = 0;
      avg_bitrate_bps = 0.0;
    }

  public:
    mstime_t start_timestamp = 0;
    mstime_t end_timestamp = 0;
    std::size_t block_count = 0;
    std::size_t file_size = 0;
    double avg_bitrate_bps = 0.0;
  };

  // Options 인터페이스: 세그먼트 생성에 필요한 설정 정보
  class Options
  {
  public:
    virtual ~Options() = default;

    virtual int64_t channel_id() const = 0;

    // 세그먼트 파일을 저장할 루트 디렉토리 (절대 또는 상대 경로)
    virtual const std::string& root_directory() const = 0;

    // 세그먼트 파일명
    // 파라미터: utc_timestamp - UTC 기반 Unix timestamp (밀리초)
    //          sequence - 시퀀스 번호 (0이면 시퀀스 번호 없음)
    //       변환 실패 시 빈 문자열 반환
    virtual std::string make_segment_filename(mstime_t utc_timestamp,
                                              int sequence = 0) const = 0;

    // 세그먼트 파일의 전체 경로를 생성합니다.
    // 파라미터: utc_timestamp - UTC 기반 Unix timestamp (밀리초)
    //          sequence - 시퀀스 번호 (0이면 시퀀스 번호 없음)
    //       변환 실패 시 빈 문자열 반환
    virtual std::string make_segment_path(mstime_t utc_timestamp,
                                          int sequence = 0) const = 0;

    // 세그먼트 헤더 생성
    virtual SegmentHeader create_segment_header() const = 0;

    // 세그먼트 종료 여부 결정
    // 파라미터: ctx - 현재 세그먼트의 컨텍스트 정보
    // 반환: true면 세그먼트 종료, false면 계속 기록
    virtual bool should_finish_segment(const Context& ctx) const = 0;

    virtual DataSyncLevel data_sync_level() const { return DataSyncLevel::kFull; }

    // 몇 블록마다 디스크 동기화를 수행할지 결정
    // 반환값: 1 = 매 블록, N = N블록마다 배치 sync
    virtual std::size_t sync_blocks() const { return 1; }

    virtual void on_sync_performed(DataSyncLevel, bool, bool) {}

    virtual void on_segment_file_conflict(const std::string&, mstime_t, int) {}

    virtual int get_next_sequence(const Context&) const { return 0; }

    virtual void on_segment_finished(const std::string&, const Context&,
                                     std::vector<IndexEntry>&&)
    {
    }

    // Timeout 여부 판단 (기본: timeout 없음)
    // 반환값: true = timeout 발생, false = timeout 아님
    virtual bool is_timed_out(const Context&) const { return false; }

    // Timeout 발생 알림 콜백 (기본: 아무것도 하지 않음)
    virtual void on_timeout_close(const std::string&) {}
  };

public:
  explicit SegmentBuilder(std::shared_ptr<Options> options);
  ~SegmentBuilder();

  NX_NON_COPYABLE_AND_MOVABLE(SegmentBuilder);

public:
  nx::expected<std::size_t> write_block(const DataBlock& block);
  void close_current_segment();
  std::string get_current_segment_path() const;
  const Context& context() const;

  // 미동기화 블록을 즉시 디스크에 동기화
  std::error_code flush_pending();

  // 미동기화 블록 수 조회
  std::size_t unflushed_blocks() const { return m_unflushed_blocks; }

  // 현재까지 축적된 인-메모리 키프레임 인덱스 스냅샷
  // footer 기록 전에도 조회 가능 (블록 기록 시 자동 축적)
  std::vector<IndexEntry> get_pending_indices() const;

  // Options 접근자 (timeout 체크용)
  std::shared_ptr<Options> get_options() const { return m_options; }

private:
  std::error_code create_new_segment(mstime_t utc_timestamp);
  std::error_code write_segment_header();
  std::error_code check_and_rotate_segment(const DataBlock& next_block);
  std::error_code sync_to_disk();
  std::error_code write_segment_footer();

private:
  std::shared_ptr<Options> m_options;
  nx::file::FileStream m_file;
  std::string m_current_path;
  Context m_ctx;
  int m_current_sequence;
  std::vector<IndexEntry> m_index_entries;
  std::size_t m_unflushed_blocks = 0;
};

} // namespace record
} // namespace nx
