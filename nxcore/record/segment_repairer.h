// 파일: segment_repairer.h
// 생성일: 2026-04-08
// 설명: 미완성 세그먼트 파일 복구 (footer 재생성)

#pragma once

#include "record.h"
#include "record_error.h"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

namespace nx {
namespace record {

// 복구 결과
struct RepairResult
{
  bool repaired = false;           // 실제 복구가 수행되었는지 (false = 이미 정상)
  int64_t channel_id = 0;          // 세그먼트 헤더에서 읽은 채널 ID
  mstime_t start_timestamp = 0;    // 첫 블록의 시작 타임스탬프
  mstime_t end_timestamp = 0;      // 마지막 유효 블록의 종료 타임스탬프
  std::size_t block_count = 0;     // 유효 블록 수
  std::size_t file_size = 0;       // 복구 후 최종 파일 크기
  double avg_bitrate_bps = 0.0;    // 평균 비트레이트 (bps)
  std::vector<IndexEntry> indices; // 키프레임 인덱스 목록
};

// footer 검사 결과
enum class FooterStatus
{
  kValid,     // footer 정상
  kMissing,   // footer 없음 (미완성 세그먼트)
  kCorrupted, // footer가 있으나 매직 넘버 불일치
};

// ============================================================================
// SegmentRepairer
// ============================================================================
// 단일 세그먼트 파일의 footer 검사 및 복구를 수행합니다.
// 순수 파일 I/O만 사용하며 DB 의존성이 없습니다.

class SegmentRepairer
{
public:
  // footer 유효성만 검사 (파일 끝 16바이트)
  static nx::expected<FooterStatus> check_footer(const std::filesystem::path& file_path);

  // 미완성 세그먼트 복구: 블록 순차 스캔 → truncate → footer 기록
  static nx::expected<RepairResult>
  repair_segment(const std::filesystem::path& file_path);

private:
  // 블록 순차 스캔으로 인덱스를 수집하고 마지막 유효 위치를 반환
  struct ScanResult
  {
    int64_t channel_id = 0;
    mstime_t start_timestamp = 0;
    mstime_t end_timestamp = 0;
    std::size_t block_count = 0;
    std::size_t valid_end_offset = 0; // 마지막 유효 블록 종료 오프셋
    std::vector<IndexEntry> indices;
  };

  static nx::expected<ScanResult> scan_blocks(const std::filesystem::path& file_path);

  // 파일을 truncate 후 footer 기록
  static std::error_code write_footer_at(const std::filesystem::path& file_path,
                                         std::size_t truncate_offset,
                                         const std::vector<IndexEntry>& indices);
};

} // namespace record
} // namespace nx
