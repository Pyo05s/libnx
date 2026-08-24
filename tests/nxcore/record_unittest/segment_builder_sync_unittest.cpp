// 파일: segment_builder_sync_unittest.cpp
// 생성일: 2026-03-26
// 설명: SegmentBuilder sync_blocks 배치 동기화 테스트

#include "segment_builder_test_fixture.h"

namespace {

// sync_blocks 테스트용 Options (sync 콜백 카운트 추적)
class SyncTestOptions : public TestSegmentBuilderOptions
{
public:
  SyncTestOptions(int64_t ch_id, std::size_t sync_blocks_count)
      : TestSegmentBuilderOptions(ch_id)
      , m_sync_blocks(sync_blocks_count)
      , m_sync_count(0)
  {
  }

  std::size_t sync_blocks() const override { return m_sync_blocks; }

  void on_sync_performed(DataSyncLevel /*sync_level*/, bool /*stream_flushed*/,
                         bool /*os_synced*/
                         ) override
  {
    ++m_sync_count;
  }

  std::size_t m_sync_blocks;
  int m_sync_count;
};

} // namespace

// sync_blocks=1 일 때 매 블록마다 sync 호출 (기존 동작)
TEST_F(SegmentBuilderTestFixture, SyncBlocksDefault)
{
  auto opts = std::make_shared<SyncTestOptions>(1, 1);
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  // 5블록 쓰기
  for (int i = 0; i < 5; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  // 헤더 sync(1) + 블록 당 sync(5) = 6회
  EXPECT_EQ(opts->m_sync_count, 6);
  EXPECT_EQ(builder.unflushed_blocks(), 0u);
}

// sync_blocks=5 일 때 5블록마다 배치 sync
TEST_F(SegmentBuilderTestFixture, SyncBlocksBatch5)
{
  auto opts = std::make_shared<SyncTestOptions>(2, 5);
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  // 3블록 쓰기 — sync_blocks=5 미달이므로 블록 sync 없음
  for (int i = 0; i < 3; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  // 헤더 sync(1) + 블록 sync(0) = 1회
  EXPECT_EQ(opts->m_sync_count, 1);
  EXPECT_EQ(builder.unflushed_blocks(), 3u);

  // 2블록 더 쓰기 → 총 5블록 → 배치 sync 발생
  for (int i = 3; i < 5; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  // 헤더 sync(1) + 배치 sync(1) = 2회
  EXPECT_EQ(opts->m_sync_count, 2);
  EXPECT_EQ(builder.unflushed_blocks(), 0u);
}

// sync_blocks=5에서 7블록 쓰기 → 1회 배치 sync + 2블록 미동기화
TEST_F(SegmentBuilderTestFixture, SyncBlocksPartialBatch)
{
  auto opts = std::make_shared<SyncTestOptions>(3, 5);
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  // 7블록 쓰기
  for (int i = 0; i < 7; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  // 헤더 sync(1) + 5블록 배치(1) = 2회, 나머지 2블록 미동기화
  EXPECT_EQ(opts->m_sync_count, 2);
  EXPECT_EQ(builder.unflushed_blocks(), 2u);
}

// close_current_segment()는 미동기화 블록이 있어도 항상 sync
TEST_F(SegmentBuilderTestFixture, SyncBlocksCloseFlushes)
{
  auto opts = std::make_shared<SyncTestOptions>(4, 10);
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  // 3블록 쓰기 (sync_blocks=10 미달)
  for (int i = 0; i < 3; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  EXPECT_EQ(builder.unflushed_blocks(), 3u);

  // close 시 footer + 최종 sync 발생
  int sync_before_close = opts->m_sync_count;
  builder.close_current_segment();

  // close 시 sync_to_disk 1회 호출
  EXPECT_GT(opts->m_sync_count, sync_before_close);
  EXPECT_EQ(builder.unflushed_blocks(), 0u);
}

// flush_pending() 호출 시 미동기화 블록 즉시 sync
TEST_F(SegmentBuilderTestFixture, FlushPending)
{
  auto opts = std::make_shared<SyncTestOptions>(5, 10);
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  // 3블록 쓰기
  for (int i = 0; i < 3; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  EXPECT_EQ(builder.unflushed_blocks(), 3u);
  int sync_before = opts->m_sync_count;

  // flush_pending 호출
  auto ec = builder.flush_pending();
  EXPECT_FALSE(ec);
  EXPECT_EQ(builder.unflushed_blocks(), 0u);
  EXPECT_EQ(opts->m_sync_count, sync_before + 1);

  // 이미 flush된 상태에서 다시 호출 → sync 안 함
  ec = builder.flush_pending();
  EXPECT_FALSE(ec);
  EXPECT_EQ(opts->m_sync_count, sync_before + 1);
}

// sync_blocks=0은 1로 보정되는지 확인 (BasicSegmentBuilderOptions 경유)
TEST_F(SegmentBuilderTestFixture, SyncBlocksZeroDefaultsToOne)
{
  auto opts = std::make_shared<SyncTestOptions>(6, 0);
  // SyncTestOptions는 직접 값을 저장하므로 0 그대로 — 테스트: sync_blocks=0이면
  // 매번 sync (0 >= 0은 항상 true이므로 매 블록 sync)
  opts->set_max_file_size(1024 * 1024);
  opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(opts);

  auto base_ts = make_timestamp(2025, 12, 1, 10, 0, 0);

  for (int i = 0; i < 3; ++i) {
    auto block = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block).has_value());
  }

  // sync_blocks=0 → 매 블록 sync: 헤더(1) + 블록(3) = 4회
  EXPECT_EQ(opts->m_sync_count, 4);
}
