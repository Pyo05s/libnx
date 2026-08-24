// 파일: segment_repairer_unittest.cpp
// 생성일: 2026-04-08
// 설명: SegmentRepairer 단위 테스트

#include "nxcore/record/segment_repairer.h"
#include "segment_builder_test_fixture.h"


#include <cstring>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>


namespace fs = std::filesystem;

using namespace nx::record;

// ============================================================================
// 테스트 픽스처
// ============================================================================

class SegmentRepairerTest : public SegmentBuilderTestFixture
{
protected:
  static constexpr int64_t kTestChannelId = 42;
  static constexpr std::size_t kPayloadSize = 100;

  const fs::path m_test_dir = "test_segments_repairer";

  void SetUp() override
  {
    std::error_code ec;
    fs::remove_all(m_test_dir, ec);
    fs::create_directories(m_test_dir, ec);
  }

  void TearDown() override
  {
    std::error_code ec;
    fs::remove_all(m_test_dir, ec);
  }

  // 완전한 세그먼트 파일 생성 (헤더 + 블록 N개 + footer)
  fs::path create_complete_segment(int block_count, bool with_keyframes = true)
  {
    auto path = m_test_dir / "complete.nxb";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    write_segment_header(file);

    mstime_t base_ts = make_timestamp(2026, 4, 8, 12, 0, 0);
    std::vector<IndexEntry> indices;

    for (int i = 0; i < block_count; ++i) {
      mstime_t start = base_ts + i * 1000;
      mstime_t end = start + 999;
      bool is_keyframe = with_keyframes && (i % 3 == 0);
      auto offset = static_cast<uint64_t>(file.tellp());
      write_data_block(file, start, end, is_keyframe);

      if (is_keyframe) {
        IndexEntry idx{};
        idx.magic = IndexEntry::kMagic;
        idx.flags = static_cast<uint32_t>(BlockFlags::kHasKeyFrame);
        idx.timestamp = start;
        idx.offset = offset;
        indices.push_back(idx);
      }
    }

    write_footer(file, indices);
    file.close();
    return path;
  }

  // 미완성 세그먼트 파일 생성 (헤더 + 블록 N개, footer 없음)
  fs::path create_incomplete_segment(int block_count, bool with_keyframes = true)
  {
    auto path = m_test_dir / "incomplete.nxb";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    write_segment_header(file);

    mstime_t base_ts = make_timestamp(2026, 4, 8, 12, 0, 0);
    for (int i = 0; i < block_count; ++i) {
      mstime_t start = base_ts + i * 1000;
      mstime_t end = start + 999;
      bool is_keyframe = with_keyframes && (i % 3 == 0);
      write_data_block(file, start, end, is_keyframe);
    }

    file.close();
    return path;
  }

  // 블록 중간이 잘린 세그먼트 (헤더 + 완전 블록 N개 + 불완전 블록 1개)
  fs::path create_truncated_block_segment(int complete_blocks)
  {
    auto path = m_test_dir / "truncated_block.nxb";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);

    write_segment_header(file);

    mstime_t base_ts = make_timestamp(2026, 4, 8, 12, 0, 0);
    for (int i = 0; i < complete_blocks; ++i) {
      mstime_t start = base_ts + i * 1000;
      mstime_t end = start + 999;
      write_data_block(file, start, end, (i % 3 == 0));
    }

    // 불완전 블록: 헤더만 기록하고 데이터/종료 매직 없음
    BlockHeader incomplete_hdr{};
    incomplete_hdr.magic = BlockHeader::kMagic;
    incomplete_hdr.header_size = static_cast<uint16_t>(sizeof(BlockHeader));
    incomplete_hdr.flags = 0;
    incomplete_hdr.length =
      static_cast<uint32_t>(sizeof(BlockHeader) + 200 + sizeof(uint16_t));
    incomplete_hdr.start_timestamp = base_ts + complete_blocks * 1000;
    incomplete_hdr.end_timestamp = incomplete_hdr.start_timestamp + 999;
    file.write(reinterpret_cast<const char*>(&incomplete_hdr), sizeof(BlockHeader));

    // 일부 페이로드만 기록 (200바이트 중 50바이트만)
    std::vector<uint8_t> partial_payload(50, 0xAB);
    file.write(reinterpret_cast<const char*>(partial_payload.data()),
               static_cast<std::streamsize>(partial_payload.size()));

    file.close();
    return path;
  }

  // 헤더만 있는 세그먼트 (블록 없음)
  fs::path create_header_only_segment()
  {
    auto path = m_test_dir / "header_only.nxb";
    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    write_segment_header(file);
    file.close();
    return path;
  }

private:
  void write_segment_header(std::ofstream& file)
  {
    SegmentHeader header{};
    header.magic = SegmentHeader::kMagic;
    header.header_size = static_cast<uint16_t>(sizeof(SegmentHeader));
    header.version = FormatVersion::kFormatVersion;
    header.channel_id = kTestChannelId;
    header.extension_header_size = 0;
    file.write(reinterpret_cast<const char*>(&header), sizeof(SegmentHeader));
  }

  void write_data_block(std::ofstream& file, mstime_t start, mstime_t end,
                        bool is_keyframe)
  {
    // 블록 = BlockHeader + 페이로드 + kBlockEndMagic
    std::size_t total = sizeof(BlockHeader) + kPayloadSize + sizeof(uint16_t);

    BlockHeader hdr{};
    hdr.magic = BlockHeader::kMagic;
    hdr.header_size = static_cast<uint16_t>(sizeof(BlockHeader));
    hdr.flags = is_keyframe ? static_cast<uint32_t>(BlockFlags::kHasKeyFrame) : 0;
    hdr.length = static_cast<uint32_t>(total);
    hdr.start_timestamp = start;
    hdr.end_timestamp = end;

    file.write(reinterpret_cast<const char*>(&hdr), sizeof(BlockHeader));

    // 페이로드 (VideoBlockEntry 시뮬레이션: type=Video, codec=H264)
    std::vector<uint8_t> payload(kPayloadSize, 0x00);
    if (kPayloadSize >= sizeof(VideoBlockEntry)) {
      VideoBlockEntry ve{};
      ve.type = static_cast<uint8_t>(EntryType::kVideo);
      ve.archive_type = 0;
      ve.header_size = static_cast<uint16_t>(sizeof(VideoBlockEntry));
      ve.timestamp = start;
      ve.codec_type = static_cast<uint8_t>(VideoCodecType::kH264);
      ve.frame_type = is_keyframe ? static_cast<uint8_t>(VideoFrameType::kIFrame)
                                  : static_cast<uint8_t>(VideoFrameType::kPFrame);
      ve.reserved1[0] = 0;
      ve.reserved1[1] = 0;
      ve.payload_size = static_cast<uint32_t>(kPayloadSize - sizeof(VideoBlockEntry));
      std::memcpy(payload.data(), &ve, sizeof(VideoBlockEntry));
    }
    file.write(reinterpret_cast<const char*>(payload.data()),
               static_cast<std::streamsize>(payload.size()));

    uint16_t end_magic = kBlockEndMagic;
    file.write(reinterpret_cast<const char*>(&end_magic), sizeof(uint16_t));
  }

  void write_footer(std::ofstream& file, const std::vector<IndexEntry>& indices)
  {
    for (const auto& entry : indices) {
      file.write(reinterpret_cast<const char*>(&entry), sizeof(IndexEntry));
    }

    FooterHeader footer{};
    footer.magic = FooterHeader::kMagicStart;
    footer.header_size = static_cast<uint16_t>(sizeof(FooterHeader));
    footer.index_count = static_cast<uint32_t>(indices.size());
    footer.index_size = static_cast<uint32_t>(sizeof(IndexEntry) * indices.size());
    footer.reserved[0] = 0;
    footer.reserved[1] = 0;
    footer.magic_end = FooterHeader::kMagicEnd;
    file.write(reinterpret_cast<const char*>(&footer), sizeof(FooterHeader));
  }
};

// ============================================================================
// check_footer 테스트
// ============================================================================

TEST_F(SegmentRepairerTest, CheckFooter_ValidSegment_ReturnsValid)
{
  auto path = create_complete_segment(5);
  auto result = SegmentRepairer::check_footer(path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, FooterStatus::kValid);
}

TEST_F(SegmentRepairerTest, CheckFooter_IncompleteSegment_ReturnsMissing)
{
  auto path = create_incomplete_segment(5);
  auto result = SegmentRepairer::check_footer(path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, FooterStatus::kMissing);
}

TEST_F(SegmentRepairerTest, CheckFooter_HeaderOnly_ReturnsMissing)
{
  auto path = create_header_only_segment();
  auto result = SegmentRepairer::check_footer(path);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, FooterStatus::kMissing);
}

TEST_F(SegmentRepairerTest, CheckFooter_NonexistentFile_ReturnsError)
{
  auto result = SegmentRepairer::check_footer(m_test_dir / "nonexistent.nxb");
  ASSERT_FALSE(result.has_value());
}

// ============================================================================
// repair_segment 테스트
// ============================================================================

TEST_F(SegmentRepairerTest, RepairSegment_AlreadyComplete_NoRepair)
{
  auto path = create_complete_segment(5);
  auto result = SegmentRepairer::repair_segment(path);

  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->repaired);
}

TEST_F(SegmentRepairerTest, RepairSegment_IncompleteSegment_Success)
{
  constexpr int kBlockCount = 10;
  auto path = create_incomplete_segment(kBlockCount);

  // 복구 전 footer 확인
  auto before = SegmentRepairer::check_footer(path);
  ASSERT_TRUE(before.has_value());
  EXPECT_EQ(*before, FooterStatus::kMissing);

  // 복구 실행
  auto result = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->repaired);
  EXPECT_EQ(result->channel_id, kTestChannelId);
  EXPECT_EQ(result->block_count, kBlockCount);
  EXPECT_GT(result->file_size, 0u);
  EXPECT_GT(result->start_timestamp, 0);
  EXPECT_GT(result->end_timestamp, result->start_timestamp);

  // 키프레임 인덱스 확인 (매 3번째 블록)
  // 블록 0, 3, 6, 9 → 4개
  EXPECT_EQ(result->indices.size(), 4u);
  for (const auto& idx : result->indices) {
    EXPECT_EQ(idx.magic, IndexEntry::kMagic);
    EXPECT_GT(idx.offset, 0u);
  }

  // 복구 후 footer 확인
  auto after = SegmentRepairer::check_footer(path);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(*after, FooterStatus::kValid);
}

TEST_F(SegmentRepairerTest, RepairSegment_TruncatedBlock_RecoversPriorBlocks)
{
  constexpr int kCompleteBlocks = 5;
  auto path = create_truncated_block_segment(kCompleteBlocks);

  auto result = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->repaired);
  EXPECT_EQ(result->block_count, kCompleteBlocks);

  // 불완전 블록은 제거되고 footer가 기록되었는지 확인
  auto after = SegmentRepairer::check_footer(path);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(*after, FooterStatus::kValid);
}

TEST_F(SegmentRepairerTest, RepairSegment_HeaderOnly_ReturnsError)
{
  auto path = create_header_only_segment();

  auto result = SegmentRepairer::repair_segment(path);
  ASSERT_FALSE(result.has_value());
  // 유효 블록이 0개이므로 복구 불가
}

TEST_F(SegmentRepairerTest, RepairSegment_BitrateCalculation)
{
  auto path = create_incomplete_segment(10);

  auto result = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->repaired);
  EXPECT_GT(result->avg_bitrate_bps, 0.0);
}

TEST_F(SegmentRepairerTest, RepairSegment_IdempotentRepair)
{
  auto path = create_incomplete_segment(5);

  // 첫 번째 복구
  auto result1 = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result1.has_value());
  EXPECT_TRUE(result1->repaired);

  // 두 번째 복구 시도 — 이미 완성이므로 repaired=false
  auto result2 = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result2.has_value());
  EXPECT_FALSE(result2->repaired);
}

TEST_F(SegmentRepairerTest, RepairSegment_NoKeyframes_EmptyIndices)
{
  auto path = create_incomplete_segment(5, /*with_keyframes=*/false);

  auto result = SegmentRepairer::repair_segment(path);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->repaired);
  EXPECT_EQ(result->block_count, 5u);
  EXPECT_TRUE(result->indices.empty());

  // footer는 인덱스 0개로도 정상 기록
  auto after = SegmentRepairer::check_footer(path);
  ASSERT_TRUE(after.has_value());
  EXPECT_EQ(*after, FooterStatus::kValid);
}
