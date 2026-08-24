// 파일: segment_builder_options.h
// 생성일: 2026-01-14
// 설명: SegmentBuilder Options 기본 구현체

#pragma once

#include "record.h"
#include "segment_builder.h"

#include <nxcore/util/time_util.h>

#include <cstddef>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace nx {
namespace record {

// ========================================================================
// BasicSegmentBuilderOptions: SegmentBuilder::Options 기본 구현
// ========================================================================
//
// 세그먼트 완료 조건에 대한 표준 구현을 제공합니다.
// - 시간 기반: max_duration 경과 시 완료
// - 크기 기반: file_size가 max_size 이상 시 완료
//
// 확장 포인트:
// - should_finish_on_duration(): 시간 조건 만족 시 추가 검사
// - should_finish_on_size(): 크기 조건 만족 시 추가 검사
// - make_segment_filename(): 세그먼트 파일명 생성
// - make_segment_path(): 세그먼트 전체 경로 생성
// - create_segment_header(): 세그먼트 헤더 생성
//
// 사용 예:
//   auto options = std::make_shared<BasicSegmentBuilderOptions>(
//       "segments",                      // root_directory
//       nx::minutes(10),        // 최대 10분
//       100 * 1024 * 1024                // 최대 100MB
//   );
//   SegmentBuilder builder(options);
//
class BasicSegmentBuilderOptions : public SegmentBuilder::Options
{
public:
  using Context = SegmentBuilder::Context;

  // 생성자
  explicit BasicSegmentBuilderOptions(int64_t channel_id, std::string device_guid,
                                      std::string_view root_directory,
                                      nx::milliseconds max_duration = nx::minutes(10),
                                      std::size_t max_size_bytes = 100 * 1024 * 1024)
      : m_channel_id(channel_id)
      , m_device_guid(std::move(device_guid))
      , m_root_directory(root_directory)
      , m_max_duration(max_duration)
      , m_max_size_bytes(max_size_bytes)
      , m_sync_level(DataSyncLevel::kFull)
      , m_sync_blocks(1)
  {
  }

  ~BasicSegmentBuilderOptions() override = default;

  // ========================================================================
  // SegmentBuilder::Options 인터페이스 구현
  // ========================================================================
  int64_t channel_id() const override { return m_channel_id; }

  const std::string& device_guid() const { return m_device_guid; }

  const std::string& root_directory() const override { return m_root_directory; }

  std::string make_segment_filename(mstime_t utc_timestamp, int sequence) const override
  {
    nx::Timestamp ts(utc_timestamp);

    // Timestamp::format_local을 사용하여 로컬 시간 포맷팅
    auto format_result = ts.format_local("%Y%m%d_%H%M%S");
    if (!format_result) {
      return "";
    }

    auto prefix = filename_prefix();

    // 파일명 생성: prefix_YYYYMMDD_HHMMSS
    std::string filename =
      prefix.empty() ? format_result.value() : prefix + "_" + format_result.value();

    // 시퀀스 번호 추가 (1 이상일 때)
    if (sequence > 0) {
      char seq_buf[8];
      std::snprintf(seq_buf, sizeof(seq_buf), "_%03d", sequence);
      filename += seq_buf;
    }

    // 확장자 추가
    filename += ".nxb";

    return filename;
  }

  std::string make_segment_path(mstime_t utc_timestamp, int sequence) const override
  {
    std::string root = root_directory();
    if (root.empty()) {
      return "";
    }

    nx::Timestamp ts(utc_timestamp);

    // 날짜 디렉토리 이름: YYYYMMDD
    auto date_result = ts.format_local("%Y%m%d");
    if (!date_result) {
      return "";
    }

    // 파일명 생성 (시퀀스 포함)
    std::string filename = make_segment_filename(utc_timestamp, sequence);
    if (filename.empty()) {
      return "";
    }

    // 전체 경로 조합: root/<device_guid>/<YYYYMMDD>/<filename>
    fs::path full_path = fs::path(root) / m_device_guid / date_result.value() / filename;

    return full_path.string();
  }

  SegmentHeader create_segment_header() const override
  {
    SegmentHeader header;
    header.magic = SegmentHeader::kMagic;
    header.header_size = static_cast<uint16_t>(sizeof(SegmentHeader));
    header.version = FormatVersion::kFormatVersion;
    header.channel_id = channel_id();
    header.extension_header_size = 0;
    return header;
  }

  bool should_finish_segment(const Context& ctx) const override
  {
    // 각 완료 조건 검사 (확장 포인트 호출)
    if (should_finish_on_duration(ctx)) {
      return true;
    }

    if (should_finish_on_size(ctx)) {
      return true;
    }

    return false;
  }

  DataSyncLevel data_sync_level() const override { return m_sync_level; }

  std::size_t sync_blocks() const override { return m_sync_blocks; }

  // ========================================================================
  // 설정 메서드
  // ========================================================================

  void set_max_duration(nx::milliseconds duration) { m_max_duration = duration; }

  void set_max_size(std::size_t bytes) { m_max_size_bytes = bytes; }

  void set_data_sync_level(DataSyncLevel level) { m_sync_level = level; }

  void set_sync_blocks(std::size_t blocks) { m_sync_blocks = (blocks > 0) ? blocks : 1; }

  nx::milliseconds get_max_duration() const { return m_max_duration; }

  std::size_t get_max_size() const { return m_max_size_bytes; }

protected:
  // ========================================================================
  // 확장 포인트: 파생 클래스에서 추가 조건 구현 가능
  // ========================================================================

  // 세그먼트 파일명 접두사 (오버라이드 가능)
  virtual std::string filename_prefix() const { return "segment"; }

  // 시간 기반 완료 조건 검사
  // 반환값: true = 세그먼트 완료, false = 계속 대기
  virtual bool should_finish_on_duration(const Context& ctx) const
  {
    if (ctx.start_timestamp >= 0 && ctx.end_timestamp >= 0) {
      mstime_t duration_ms = ctx.end_timestamp - ctx.start_timestamp;
      mstime_t max_duration_ms = m_max_duration.count();
      return duration_ms >= max_duration_ms;
    }
    return false;
  }

  // 크기 기반 완료 조건 검사
  // 반환값: true = 세그먼트 완료, false = 계속 대기
  virtual bool should_finish_on_size(const Context& ctx) const
  {
    return ctx.file_size >= m_max_size_bytes;
  }

private:
  int64_t m_channel_id;
  std::string m_device_guid;
  std::string m_root_directory;
  nx::milliseconds m_max_duration;
  std::size_t m_max_size_bytes;
  DataSyncLevel m_sync_level;
  std::size_t m_sync_blocks;
};

} // namespace record
} // namespace nx
