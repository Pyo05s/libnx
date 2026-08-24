// 파일: segment_builder_basic_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder 기본 기능 테스트

#include "segment_builder_test_fixture.h"

// 테스트 1: 기본 블록 쓰기
TEST_F(SegmentBuilderTestFixture, WriteBlockBasic)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(1);
  SegmentBuilder builder(builder_opts);

  auto timestamp = make_timestamp(2025, 12, 1, 10, 0, 0);
  DataBlock block = make_test_block(timestamp, timestamp + 1000);

  auto result = builder.write_block(block);
  EXPECT_TRUE(result.has_value());

  std::string path = builder.get_current_segment_path();
  EXPECT_FALSE(path.empty());
  EXPECT_TRUE(verify_file_exists(path));
  EXPECT_TRUE(verify_segment_header(path, 1));
}

// 테스트 2: 여러 블록 쓰기
TEST_F(SegmentBuilderTestFixture, WriteMultipleBlocks)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(2);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 14, 30, 0);

  for (int i = 0; i < 3; ++i) {
    DataBlock block
      = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 50);
    EXPECT_TRUE(builder.write_block(block));
  }

  const auto& ctx = builder.context();
  EXPECT_EQ(ctx.block_count, 3u);
  EXPECT_GT(ctx.file_size, sizeof(SegmentHeader));
}

// 테스트 5: 컨텍스트 업데이트 확인
TEST_F(SegmentBuilderTestFixture, ContextUpdate)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(5);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 13, 0, 0);

  auto ctx1 = builder.context();
  EXPECT_EQ(ctx1.block_count, 0u);
  EXPECT_EQ(ctx1.file_size, 0u);

  DataBlock block1 = make_test_block(base_ts, base_ts + 5000, 100);
  EXPECT_TRUE(builder.write_block(block1));

  auto ctx2 = builder.context();
  EXPECT_EQ(ctx2.block_count, 1u);
  EXPECT_EQ(ctx2.start_timestamp, base_ts);
  EXPECT_EQ(ctx2.end_timestamp, base_ts + 5000);
  EXPECT_GT(ctx2.file_size, sizeof(SegmentHeader));

  DataBlock block2 = make_test_block(base_ts + 10000, base_ts + 15000, 100);
  EXPECT_TRUE(builder.write_block(block2));

  auto ctx3 = builder.context();
  EXPECT_EQ(ctx3.block_count, 2u);
  EXPECT_EQ(ctx3.end_timestamp, base_ts + 15000);
  EXPECT_GT(ctx3.file_size, ctx2.file_size);
}

// 테스트 6: 세그먼트 닫기
TEST_F(SegmentBuilderTestFixture, CloseSegment)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(6);
  SegmentBuilder builder(builder_opts);

  auto timestamp = make_timestamp(2025, 12, 1, 15, 0, 0);
  DataBlock block = make_test_block(timestamp, timestamp + 1000);

  EXPECT_TRUE(builder.write_block(block));
  std::string path = builder.get_current_segment_path();
  EXPECT_FALSE(path.empty());

  builder.close_current_segment();

  const auto& ctx = builder.context();
  EXPECT_EQ(ctx.block_count, 0u);
  EXPECT_EQ(ctx.file_size, 0u);
  EXPECT_TRUE(builder.get_current_segment_path().empty());
  EXPECT_TRUE(verify_file_exists(path));
}

// 테스트 7: 빈 블록 거부
TEST_F(SegmentBuilderTestFixture, RejectEmptyBlock)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(7);
  SegmentBuilder builder(builder_opts);

  DataBlock empty_block;
  EXPECT_FALSE(builder.write_block(empty_block));
}

// 테스트 8: 다중 채널
TEST_F(SegmentBuilderTestFixture, MultipleChannels)
{
  auto builder_opts1 = std::make_shared<TestSegmentBuilderOptions>(100);
  auto builder_opts2 = std::make_shared<TestSegmentBuilderOptions>(200);

  SegmentBuilder builder1(builder_opts1);
  SegmentBuilder builder2(builder_opts2);

  auto timestamp = make_timestamp(2025, 12, 1, 16, 0, 0);
  DataBlock block = make_test_block(timestamp, timestamp + 1000);

  EXPECT_TRUE(builder1.write_block(block));
  EXPECT_TRUE(builder2.write_block(block));

  std::string path1 = builder1.get_current_segment_path();
  std::string path2 = builder2.get_current_segment_path();

  EXPECT_NE(path1, path2);
  EXPECT_TRUE(verify_segment_header(path1, 100));
  EXPECT_TRUE(verify_segment_header(path2, 200));
}

// 테스트 9: 파일 내용 검증
TEST_F(SegmentBuilderTestFixture, VerifyFileContent)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(8);
  SegmentBuilder builder(builder_opts);

  auto timestamp = make_timestamp(2025, 12, 1, 17, 0, 0);
  DataBlock block = make_test_block(timestamp, timestamp + 1000, 200);

  EXPECT_TRUE(builder.write_block(block));

  std::string path = builder.get_current_segment_path();
  builder.close_current_segment();

  std::ifstream file(path, std::ios::binary);
  ASSERT_TRUE(file.is_open());

  SegmentHeader header;
  file.read(reinterpret_cast<char*>(&header), sizeof(SegmentHeader));
  EXPECT_EQ(header.magic, SegmentHeader::kMagic);
  EXPECT_EQ(header.channel_id, 8u);

  BlockHeader block_header;
  file.read(reinterpret_cast<char*>(&block_header), sizeof(BlockHeader));
  EXPECT_EQ(block_header.magic, BlockHeader::kMagic);
  EXPECT_EQ(block_header.start_timestamp, timestamp);
  EXPECT_EQ(block_header.end_timestamp, timestamp + 1000);

  file.close();
}
