// 파일: segment_builder_test_fixture.h
// 생성일: 2025-01-15
// 설명: SegmentBuilder 테스트를 위한 공통 픽스처 및 헬퍼

#pragma once

#include "nxcore/record/segment_builder.h"
#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <utility>

namespace fs = std::filesystem;

using namespace nx;
using namespace nx::record;

// 테스트용 SegmentBuilder::Options 구현
class TestSegmentBuilderOptions : public SegmentBuilder::Options
{
public:
  TestSegmentBuilderOptions(int64_t channel_id)
      : m_root_directory("test_segments_builder")
      , m_channel_id(channel_id)
      , m_max_file_size(10 * 1024) // 10KB
      , m_max_duration(60 * 1000)  // 60초
  {
  }

  const std::string& root_directory() const override { return m_root_directory; }

  // 세그먼트 파일명에 사용할 접두사(선택적)
  std::string filename_prefix() const { return "segment"; }

  std::string make_segment_filename(mstime_t utc_timestamp, int sequence) const
  {
    nx::Timestamp ts(utc_timestamp);

    // Timestamp::format_local을 사용하여 로컬 시간 포맷팅
    auto format_result = ts.format_local("%Y%m%d_%H%M");
    if (!format_result) {
      return "";
    }

    auto prefix = filename_prefix();

    // 파일명 생성: prefix_YYYYMMDD_HHMM
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

  // 반환: root/<channel_id>/<YYYYMMDD>/<filename> 형식의 전체 경로 (local time
  // 기준)
  //       변환 실패 시 빈 문자열 반환
  std::string make_segment_path(mstime_t utc_timestamp, int sequence = 0) const override
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

    // 전체 경로 조합: root/<channel_id>/<YYYYMMDD>/<filename>
    fs::path full_path =
      fs::path(root) / std::to_string(channel_id()) / date_result.value() / filename;

    return full_path.string();
  }

  // 세그먼트 헤더 생성
  SegmentHeader create_segment_header() const override
  {
    SegmentHeader header;
    header.magic = SegmentHeader::kMagic;
    header.header_size = static_cast<uint16_t>(sizeof(SegmentHeader));
    header.version = FormatVersion::kFormatVersion;
    header.channel_id = m_channel_id;
    header.extension_header_size = 0; // 현재는 확장 헤더 없음
    return header;
  }

  bool should_finish_segment(const SegmentBuilder::Context& ctx) const override
  {
    // 파일 크기 또는 시간 제한 도달 시 세그먼트 종료
    std::size_t duration = ctx.end_timestamp - ctx.start_timestamp;

    // 크기 기반 체크를 먼저 수행
    if (ctx.file_size >= m_max_file_size) {
      return true;
    }

    // 시간 기반 체크
    if (duration >= m_max_duration) {
      return true;
    }

    return false;
  }

  int get_next_sequence(const SegmentBuilder::Context& ctx) const override
  {
    // 크기 제한으로 세그먼트를 분할하는 경우 시퀀스 번호 사용
    if (ctx.file_size >= m_max_file_size && ctx.block_count > 0) {
      return 1; // 시퀀스 번호 사용
    }

    // 시간 기반이지만 1분 미만인 경우
    // 파일명이 YYYYMMDD_HHMM 형식이므로 1분 미만 세그먼트는
    // 같은 파일명을 가질 수 있어 시퀀스 번호 사용
    std::size_t duration = ctx.end_timestamp - ctx.start_timestamp;
    if (duration > 0 && duration < 60 && ctx.block_count > 0) {
      return 1; // 시퀀스 번호 사용
    }

    return 0; // 시간 기반 (1분 이상), 시퀀스 불필요
  }

  int64_t channel_id() const { return m_channel_id; }

  void set_max_file_size(std::size_t size) { m_max_file_size = size; }

  void set_max_duration(std::size_t duration) { m_max_duration = duration; }

  void set_root_directory(const std::string& dir) { m_root_directory = dir; }

private:
  std::string m_root_directory;
  int64_t m_channel_id;
  std::size_t m_max_file_size;
  std::size_t m_max_duration;
};

// 테스트 픽스처
class SegmentBuilderTestFixture : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 테스트 디렉토리 정리
    cleanup_test_directory();
  }

  void TearDown() override
  {
    // 테스트 후 정리
    cleanup_test_directory();
  }

  void cleanup_test_directory()
  {
    std::error_code ec;
    fs::remove_all("test_segments_builder", ec);
  }

  // UTC timestamp 생성 헬퍼 (밀리초 반환)
  mstime_t make_timestamp(int year, int month, int day, int hour, int min,
                          int sec = 0) const
  {
    auto result = nx::util::make_timestamp_utc(year, month, day, hour, min, sec);
    if (!result) {
      throw std::runtime_error("Failed to create timestamp");
    }
    return result.value();
  }

  // 테스트용 DataBlock 생성
  DataBlock make_test_block(mstime_t start_ts, mstime_t end_ts,
                            std::size_t payload_size = 100)
  {
    DataBlock block;

    std::size_t header_size = sizeof(BlockHeader);
    std::size_t end_magic_size = sizeof(uint16_t);
    std::size_t total_size = header_size + payload_size + end_magic_size;

    block.serialized.resize(total_size);
    uint8_t* base = block.serialized.data();

    // 헤더 작성
    BlockHeader hdr;
    hdr.magic = BlockHeader::kMagic;
    hdr.header_size = static_cast<uint16_t>(header_size);
    hdr.flags = 0;
    hdr.length = static_cast<uint32_t>(total_size);
    hdr.start_timestamp = start_ts;
    hdr.end_timestamp = end_ts;

    std::memcpy(base, &hdr, header_size);
    block.header = reinterpret_cast<BlockHeader const*>(base);

    // 페이로드 (더미 데이터)
    for (std::size_t i = 0; i < payload_size; ++i) {
      base[header_size + i] = static_cast<uint8_t>(i % 256);
    }

    // 종료 매직
    uint16_t end_magic = kBlockEndMagic;
    std::memcpy(base + header_size + payload_size, &end_magic, end_magic_size);
    block.end_magic =
      reinterpret_cast<uint16_t const*>(base + header_size + payload_size);

    return block;
  }

  // 파일 내용 검증 헬퍼
  bool verify_file_exists(const std::string& path) { return fs::exists(path); }

  bool verify_segment_header(const std::string& path, int64_t expected_channel_id)
  {
    std::ifstream file(path, std::ios::binary);
    if (!file.is_open()) {
      return false;
    }

    SegmentHeader header;
    file.read(reinterpret_cast<char*>(&header), sizeof(SegmentHeader));

    return (header.magic == SegmentHeader::kMagic) &&
           (header.channel_id == expected_channel_id) &&
           (header.version == FormatVersion::kFormatVersion);
  }
};
