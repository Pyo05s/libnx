// 파일: segment_builder_callback_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder 콜백 기능 테스트

#include "segment_builder_test_fixture.h"

namespace {

// 콜백 테스트용 Options
class CallbackTestOptions : public TestSegmentBuilderOptions
{
public:
  CallbackTestOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
      , callback_count(0)
  {
  }

  void on_segment_finished(const std::string& path, const SegmentBuilder::Context& ctx,
                           std::vector<IndexEntry>&&) override
  {
    callback_count++;
    finished_paths.push_back(path);
    finished_contexts.push_back(ctx);
  }

  int callback_count;
  std::vector<std::string> finished_paths;
  std::vector<SegmentBuilder::Context> finished_contexts;
};

// 예외를 던지는 콜백
class ExceptionTestOptions : public TestSegmentBuilderOptions
{
public:
  ExceptionTestOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
  {
  }

  void on_segment_finished(const std::string& /*path*/,
                           const SegmentBuilder::Context& /*ctx*/,
                           std::vector<IndexEntry>&& /*indices*/
                           ) override
  {
    throw std::runtime_error("Callback exception test");
  }
};

// 전환 시 콜백 테스트용 Options
class RotationCallbackOptions : public TestSegmentBuilderOptions
{
public:
  RotationCallbackOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
      , callback_count(0)
  {
  }

  void on_segment_finished(const std::string& path, const SegmentBuilder::Context& ctx,
                           std::vector<IndexEntry>&&) override
  {
    callback_count++;
    last_finished_path = path;
    last_finished_context = ctx;
  }

  int callback_count;
  std::string last_finished_path;
  SegmentBuilder::Context last_finished_context;
};

} // namespace

// 테스트 11: 세그먼트 완료 콜백
TEST_F(SegmentBuilderTestFixture, OnSegmentFinishedCallback)
{
  auto builder_opts = std::make_shared<CallbackTestOptions>(10);
  builder_opts->set_max_file_size(500);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 20, 0, 0);

  for (int i = 0; i < 5; ++i) {
    DataBlock block =
      make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 200);
    EXPECT_TRUE(builder.write_block(block));
  }

  builder.close_current_segment();

  EXPECT_GT(builder_opts->callback_count, 0);
  EXPECT_EQ(builder_opts->callback_count, builder_opts->finished_paths.size());
  EXPECT_EQ(builder_opts->callback_count, builder_opts->finished_contexts.size());

  for (const auto& path : builder_opts->finished_paths) {
    EXPECT_FALSE(path.empty());
    EXPECT_TRUE(verify_file_exists(path));
  }

  for (const auto& ctx : builder_opts->finished_contexts) {
    EXPECT_GT(ctx.block_count, 0u);
    EXPECT_GT(ctx.file_size, sizeof(SegmentHeader));
    EXPECT_GT(ctx.end_timestamp, ctx.start_timestamp);
  }
}

// 테스트 12: 콜백 예외 처리
TEST_F(SegmentBuilderTestFixture, OnSegmentFinishedExceptionHandling)
{
  auto builder_opts = std::make_shared<ExceptionTestOptions>(11);
  SegmentBuilder builder(builder_opts);

  auto timestamp = make_timestamp(2025, 12, 1, 21, 0, 0);
  DataBlock block = make_test_block(timestamp, timestamp + 1000);

  EXPECT_TRUE(builder.write_block(block));
  EXPECT_NO_THROW(builder.close_current_segment());

  const auto& ctx = builder.context();
  EXPECT_EQ(ctx.block_count, 0u);
  EXPECT_EQ(ctx.file_size, 0u);
}

// 테스트 13: 세그먼트 전환 시 콜백 호출
TEST_F(SegmentBuilderTestFixture, OnSegmentFinishedDuringRotation)
{
  auto builder_opts = std::make_shared<RotationCallbackOptions>(12);
  builder_opts->set_max_file_size(500);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 22, 0, 0);

  DataBlock block1 = make_test_block(base_ts, base_ts + 1000, 300);
  EXPECT_TRUE(builder.write_block(block1));

  int initial_callback_count = builder_opts->callback_count;

  DataBlock block2 = make_test_block(base_ts + 1000, base_ts + 2000, 300);
  EXPECT_TRUE(builder.write_block(block2));

  EXPECT_GT(builder_opts->callback_count, initial_callback_count);
  EXPECT_FALSE(builder_opts->last_finished_path.empty());
  EXPECT_TRUE(verify_file_exists(builder_opts->last_finished_path));
  EXPECT_GT(builder_opts->last_finished_context.block_count, 0u);
  EXPECT_GT(builder_opts->last_finished_context.file_size, sizeof(SegmentHeader));
}
