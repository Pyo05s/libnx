// 파일: segment_builder_index_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder 인덱스 및 푸터 기능 테스트

#include "segment_builder_test_fixture.h"

namespace {

// 키프레임 블록 생성 헬퍼
DataBlock
make_keyframe_block(mstime_t start_ts, mstime_t end_ts, std::size_t payload_size = 100)
{
  DataBlock block;

  std::size_t header_size = sizeof(BlockHeader);
  std::size_t end_magic_size = sizeof(uint16_t);
  std::size_t total_size = header_size + payload_size + end_magic_size;

  block.serialized.resize(total_size);
  uint8_t* base = block.serialized.data();

  BlockHeader hdr;
  hdr.magic = BlockHeader::kMagic;
  hdr.header_size = static_cast<uint16_t>(header_size);
  hdr.flags = static_cast<uint32_t>(BlockFlags::kHasKeyFrame);
  hdr.length = static_cast<uint32_t>(total_size);
  hdr.start_timestamp = start_ts;
  hdr.end_timestamp = end_ts;

  std::memcpy(base, &hdr, header_size);
  block.header = reinterpret_cast<BlockHeader const*>(base);

  for (std::size_t i = 0; i < payload_size; ++i) {
    base[header_size + i] = static_cast<uint8_t>(i % 256);
  }

  uint16_t end_magic = kBlockEndMagic;
  std::memcpy(base + header_size + payload_size, &end_magic, end_magic_size);
  block.end_magic = reinterpret_cast<uint16_t const*>(base + header_size + payload_size);

  return block;
}

// 콜백으로 인덱스 받기 위한 Options
class IndexCallbackOptions : public TestSegmentBuilderOptions
{
public:
  IndexCallbackOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
  {
  }

  void on_segment_finished(const std::string& /*path*/,
                           const SegmentBuilder::Context& /*ctx*/,
                           std::vector<IndexEntry>&& indices) override
  {
    received_indices = std::move(indices);
  }

  std::vector<IndexEntry> received_indices;
};

// 인덱스 검증용 Options
class IndexVerifyOptions : public TestSegmentBuilderOptions
{
public:
  IndexVerifyOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
  {
  }

  void on_segment_finished(const std::string& /*path*/,
                           const SegmentBuilder::Context& /*ctx*/,
                           std::vector<IndexEntry>&& indices) override
  {
    last_indices = std::move(indices);
  }

  std::vector<IndexEntry> last_indices;
};

} // namespace

// 테스트 16: 키프레임 블록의 인덱스 생성
TEST_F(SegmentBuilderTestFixture, IndexCreationForKeyFrames)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(15);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 10, 0, 0);

  EXPECT_TRUE(builder.write_block(make_keyframe_block(base_ts, base_ts + 1000, 100)));
  EXPECT_TRUE(builder.write_block(make_test_block(base_ts + 1000, base_ts + 2000, 100)));
  EXPECT_TRUE(
    builder.write_block(make_keyframe_block(base_ts + 2000, base_ts + 3000, 100)));
  EXPECT_TRUE(builder.write_block(make_test_block(base_ts + 3000, base_ts + 4000, 100)));
  EXPECT_TRUE(
    builder.write_block(make_keyframe_block(base_ts + 4000, base_ts + 5000, 100)));

  std::string segment_path = builder.get_current_segment_path();
  builder.close_current_segment();

  ASSERT_TRUE(verify_file_exists(segment_path));

  std::ifstream file(segment_path, std::ios::binary);
  ASSERT_TRUE(file.is_open());

  file.seekg(-static_cast<int>(sizeof(FooterHeader)), std::ios::end);
  FooterHeader footer;
  file.read(reinterpret_cast<char*>(&footer), sizeof(FooterHeader));

  EXPECT_EQ(footer.magic, FooterHeader::kMagicStart);
  EXPECT_EQ(footer.magic_end, FooterHeader::kMagicEnd);
  EXPECT_EQ(footer.index_count, 3u);
  EXPECT_EQ(footer.index_size, sizeof(IndexEntry) * 3);

  file.seekg(-static_cast<int>(sizeof(FooterHeader) + footer.index_size), std::ios::end);

  std::vector<IndexEntry> entries(footer.index_count);
  for (uint32_t i = 0; i < footer.index_count; ++i) {
    file.read(reinterpret_cast<char*>(&entries[i]), sizeof(IndexEntry));

    EXPECT_EQ(entries[i].magic, IndexEntry::kMagic);
    EXPECT_TRUE(entries[i].flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame));
    EXPECT_GT(entries[i].offset, 0u);
  }

  file.close();
}

// 테스트 17: 키프레임 없는 경우 푸터 있음 (index_count = 0)
TEST_F(SegmentBuilderTestFixture, FooterWithZeroIndexCount)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(16);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 11, 0, 0);

  for (int i = 0; i < 5; ++i) {
    DataBlock block =
      make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 100);
    EXPECT_TRUE(builder.write_block(block));
  }

  std::string segment_path = builder.get_current_segment_path();
  builder.close_current_segment();

  std::ifstream file(segment_path, std::ios::binary);
  ASSERT_TRUE(file.is_open());

  file.seekg(-static_cast<int>(sizeof(FooterHeader)), std::ios::end);
  FooterHeader footer;
  file.read(reinterpret_cast<char*>(&footer), sizeof(FooterHeader));

  EXPECT_EQ(footer.magic, FooterHeader::kMagicStart);
  EXPECT_EQ(footer.magic_end, FooterHeader::kMagicEnd);
  EXPECT_EQ(footer.index_count, 0u);
  EXPECT_EQ(footer.index_size, 0u);

  file.close();
}

// 테스트 18: 인덱스 메모리 버퍼 동작 확인
TEST_F(SegmentBuilderTestFixture, IndexMemoryBufferVerification)
{
  auto builder_opts = std::make_shared<IndexCallbackOptions>(17);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 12, 0, 0);

  for (int i = 0; i < 3; ++i) {
    EXPECT_TRUE(builder.write_block(
      make_keyframe_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 100)));
  }

  builder.close_current_segment();

  ASSERT_EQ(builder_opts->received_indices.size(), 3u);

  for (std::size_t i = 0; i < builder_opts->received_indices.size(); ++i) {
    const auto& entry = builder_opts->received_indices[i];
    EXPECT_EQ(entry.magic, IndexEntry::kMagic);
    EXPECT_TRUE(entry.flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame));
    EXPECT_EQ(entry.timestamp, base_ts + static_cast<mstime_t>(i * 1000));
    EXPECT_GT(entry.offset, 0u);
  }
}

// 테스트 19: 인덱스 엔트리 순서 확인
TEST_F(SegmentBuilderTestFixture, IndexEntryOrderVerification)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(18);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 13, 0, 0);

  std::vector<mstime_t> timestamps;
  for (int i = 0; i < 5; ++i) {
    mstime_t ts = base_ts + (i * 10 * 1000);
    timestamps.push_back(ts);
    EXPECT_TRUE(builder.write_block(make_keyframe_block(ts, ts + 1000, 100)));
  }

  std::string segment_path = builder.get_current_segment_path();
  builder.close_current_segment();

  std::ifstream file(segment_path, std::ios::binary);
  ASSERT_TRUE(file.is_open());

  file.seekg(-static_cast<int>(sizeof(FooterHeader)), std::ios::end);
  FooterHeader footer;
  file.read(reinterpret_cast<char*>(&footer), sizeof(FooterHeader));

  ASSERT_EQ(footer.index_count, timestamps.size());

  file.seekg(-static_cast<int>(sizeof(FooterHeader) + footer.index_size), std::ios::end);

  for (std::size_t i = 0; i < timestamps.size(); ++i) {
    IndexEntry entry;
    file.read(reinterpret_cast<char*>(&entry), sizeof(IndexEntry));

    EXPECT_EQ(entry.timestamp, timestamps[i]);

    if (i > 0) {
      file.seekg(-static_cast<int>(sizeof(IndexEntry) * 2), std::ios::cur);
      IndexEntry prev_entry;
      file.read(reinterpret_cast<char*>(&prev_entry), sizeof(IndexEntry));
      file.seekg(sizeof(IndexEntry), std::ios::cur);

      EXPECT_GT(entry.offset, prev_entry.offset);
    }
  }

  file.close();
}

// 테스트 20: 세그먼트 전환 시 인덱스 독립성
TEST_F(SegmentBuilderTestFixture, IndexIndependenceAcrossSegments)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(19);
  builder_opts->set_max_file_size(500);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 14, 0, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 10; ++i) {
    EXPECT_TRUE(builder.write_block(
      make_keyframe_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 200)));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  builder.close_current_segment();
  ASSERT_GT(segment_paths.size(), 1u);

  for (const auto& path : segment_paths) {
    std::ifstream file(path, std::ios::binary);
    ASSERT_TRUE(file.is_open());

    file.seekg(-static_cast<int>(sizeof(FooterHeader)), std::ios::end);
    FooterHeader footer;
    file.read(reinterpret_cast<char*>(&footer), sizeof(FooterHeader));

    EXPECT_EQ(footer.magic, FooterHeader::kMagicStart);
    EXPECT_GT(footer.index_count, 0u);

    file.seekg(-static_cast<int>(sizeof(FooterHeader) + footer.index_size),
               std::ios::end);

    for (uint32_t i = 0; i < footer.index_count; ++i) {
      IndexEntry entry;
      file.read(reinterpret_cast<char*>(&entry), sizeof(IndexEntry));

      EXPECT_EQ(entry.magic, IndexEntry::kMagic);
      EXPECT_TRUE(entry.flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame));
    }

    file.close();
  }
}

// 테스트 21: 콜백을 통한 인덱스 전달 검증
TEST_F(SegmentBuilderTestFixture, IndexDeliveryThroughCallback)
{
  auto builder_opts = std::make_shared<IndexVerifyOptions>(20);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 3, 15, 0, 0);

  std::vector<mstime_t> timestamps;
  for (int i = 0; i < 5; ++i) {
    mstime_t ts = base_ts + (i * 10 * 1000);
    timestamps.push_back(ts);
    EXPECT_TRUE(builder.write_block(make_keyframe_block(ts, ts + 1000, 100)));
  }

  builder.close_current_segment();

  ASSERT_EQ(builder_opts->last_indices.size(), timestamps.size());

  for (std::size_t i = 0; i < timestamps.size(); ++i) {
    const auto& entry = builder_opts->last_indices[i];
    EXPECT_EQ(entry.magic, IndexEntry::kMagic);
    EXPECT_EQ(entry.timestamp, timestamps[i]);
    EXPECT_TRUE(entry.flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame));
  }
}
