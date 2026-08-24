// 파일: segment_builder_bitrate_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder Bitrate 통계 테스트

#include "segment_builder_test_fixture.h"
#include <iostream>

// 테스트 27: Bitrate 계산 검증
TEST_F(SegmentBuilderTestFixture, BitrateCalculation)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(26);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 3, 18, 0, 0);

  for (int i = 0; i < 3; ++i) {
    DataBlock block
      = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 1000);
    EXPECT_TRUE(builder.write_block(block));
  }

  const auto& ctx = builder.context();

  double duration_sec
    = static_cast<double>(ctx.end_timestamp - ctx.start_timestamp) / 1000.0;
  EXPECT_GT(duration_sec, 0.0);

  double expected_bitrate = (static_cast<double>(ctx.file_size) * 8.0) / duration_sec;

  EXPECT_GT(ctx.avg_bitrate_bps, 0.0);
  EXPECT_DOUBLE_EQ(ctx.avg_bitrate_bps, expected_bitrate);

  std::cout << "\n=== Bitrate Statistics ===\n";
  std::cout << "Duration: " << duration_sec << " sec\n";
  std::cout << "File size: " << ctx.file_size << " bytes\n";
  std::cout << "Avg bitrate: " << ctx.avg_bitrate_bps << " bps\n";
  std::cout << "Avg bitrate: " << (ctx.avg_bitrate_bps / 1000.0) << " Kbps\n";
}

// 테스트 30: Context clear 시 bitrate도 초기화
TEST_F(SegmentBuilderTestFixture, ContextClearResetsBitrate)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(29);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 3, 12, 0, 0);

  DataBlock block = make_test_block(base_ts, base_ts + 1000, 1000);
  EXPECT_TRUE(builder.write_block(block));

  EXPECT_GT(builder.context().avg_bitrate_bps, 0.0);

  builder.close_current_segment();

  const auto& ctx = builder.context();
  EXPECT_EQ(ctx.start_timestamp, 0);
  EXPECT_EQ(ctx.end_timestamp, 0);
  EXPECT_EQ(ctx.block_count, 0u);
  EXPECT_EQ(ctx.file_size, 0u);
  EXPECT_DOUBLE_EQ(ctx.avg_bitrate_bps, 0.0);
}
