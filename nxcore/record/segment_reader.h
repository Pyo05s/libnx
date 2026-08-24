// 파일: segment_reader.h
// 생성일: 2026-02-27
// 설명: 단일 세그먼트 파일 바이너리 파서

#pragma once

#include "segment_reader_error.h"
#include "record.h"

#include <nxcore/util/type_util.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <system_error>
#include <vector>

#include "../file/file_stream.h"

namespace nx {
namespace record {

// ============================================================================
// 읽기 위치 및 결과 구조체
// ============================================================================

/// 세그먼트 파일 내 읽기 위치
struct SegmentPosition
{
  int64_t block_offset = 0; // 블록 시작 오프셋 (파일 내)
  int32_t entry_index = 0;  // 블록 내 엔트리 인덱스
  int64_t timestamp = 0;    // 현재 타임스탬프
};

/// 읽기 결과 프레임 (entry 단위)
struct ReadFrameResult
{
  EntryType type = {};
  int64_t timestamp = 0;
  bool is_keyframe = false;

  // 비디오 전용
  VideoCodecType video_codec = {};
  VideoFrameType frame_type = {};

  // 오디오 전용
  AudioCodecType audio_codec = {};
  uint32_t sample_rate = 0;
  uint16_t channels = 0;

  // 페이로드
  std::vector<uint8_t> payload;
};

// ============================================================================
// SegmentReader
// ============================================================================

/// 단일 세그먼트 파일 리더
class SegmentReader
{
  NX_NON_COPYABLE_AND_MOVABLE(SegmentReader);

public:
  SegmentReader();
  ~SegmentReader();

  /// 세그먼트 파일 열기
  /// @param file_path 세그먼트 파일 경로
  /// @return 에러 코드 (성공 시 빈 error_code)
  std::error_code open(const std::filesystem::path& file_path);

  /// 파일 닫기
  void close();

  /// 열려있는지 확인
  bool is_open() const;

  /// 세그먼트 헤더 조회 (open 후 유효)
  const SegmentHeader& segment_header() const;

  /// 다음 엔트리 읽기
  /// @return 프레임 결과 또는 에러 (EOF 시 SegmentReaderErrc::kEndOfSegment)
  nx::expected<ReadFrameResult> read_next_entry();

  /// 특정 오프셋으로 이동
  /// @param offset 파일 내 바이트 오프셋 (BlockHeader 시작 위치)
  std::error_code seek_to_offset(int64_t offset);

  /// 특정 시간으로 이동 (footer 인덱스 활용)
  /// @param target_time 목표 시간 (밀리초, Unix Epoch)
  std::error_code seek_to_time(int64_t target_time);

  /// 현재 읽기 위치
  SegmentPosition current_position() const;

  /// 세그먼트의 시간 범위
  int64_t start_time() const;
  int64_t end_time() const;

  /// footer 인덱스 로드 (seek 지원용)
  std::error_code load_footer_index();

private:
  /// 블록 헤더 읽기
  nx::expected<BlockHeader> read_block_header();

  /// 블록 내 엔트리 읽기
  nx::expected<ReadFrameResult> read_entry_from_block(const BlockHeader& block_header);

  /// footer에서 인덱스 엔트리 배열 파싱
  std::error_code parse_footer();

  nx::file::FileStream m_file;
  std::filesystem::path m_file_path;
  SegmentHeader m_segment_header{};
  bool m_is_open = false;

  // footer 인덱스 캐시 (seek용)
  std::vector<IndexEntry> m_footer_indices;
  bool m_footer_loaded = false;

  // 현재 읽기 상태
  SegmentPosition m_position;
  BlockHeader m_current_block_header{};
  int32_t m_entries_read_in_block = 0;
  int64_t m_block_data_end_offset = 0; // 현재 블록의 데이터 종료 위치
  bool m_need_new_block = true;        // 새 블록 읽기 필요 여부
};

} // namespace record
} // namespace nx
