// 파일: segment_builder_anomaly_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder 파일 충돌 및 시간 변경 테스트

#include "segment_builder_test_fixture.h"

namespace {

// 파일 충돌 + 시간 변경 테스트용 Options
class FileConflictTestOptions : public TestSegmentBuilderOptions
{
public:
  FileConflictTestOptions(int64_t ch_id)
      : TestSegmentBuilderOptions(ch_id)
      , conflict_count(0)
      , time_change_detected(false)
      , time_before_change(0)
      , time_after_change(0)
  {
  }

  // 시간 변경 시뮬레이션 (외부에서 호출)
  void simulate_time_change(mstime_t old_time, mstime_t new_time)
  {
    time_change_detected = true;
    time_before_change = old_time;
    time_after_change = new_time;
  }

  void on_segment_file_conflict(const std::string& conflicting_path,
                                mstime_t segment_time, int new_sequence) override
  {
    conflict_count++;
    conflicts.push_back({
      conflicting_path, segment_time, new_sequence,
      false // should_invalidate
    });

    // 시간 변경 감지 상태에서만 처리
    if (time_change_detected) {
      if (segment_time < time_before_change) {
        // 시간 역행 → 무효화
        conflicts.back().should_invalidate = true;
        invalidated_paths.push_back(conflicting_path);
      }
      else {
        // segment_time >= old_time → 플래그 리셋
        time_change_detected = false;
      }
    }
  }

  struct ConflictInfo
  {
    std::string path;
    mstime_t segment_time;
    int sequence;
    bool should_invalidate;
  };

  int conflict_count;
  bool time_change_detected;
  mstime_t time_before_change;
  mstime_t time_after_change;
  std::vector<ConflictInfo> conflicts;
  std::vector<std::string> invalidated_paths;
};

} // namespace

// 테스트 22: 파일 충돌 + 시간 역행
TEST_F(SegmentBuilderTestFixture, FileConflictWithTimeBackward)
{
  auto builder_opts = std::make_shared<FileConflictTestOptions>(21);
  SegmentBuilder builder(builder_opts);

  // 1. 정상 시간으로 여러 세그먼트 생성 (14:00 ~ 14:30)
  auto time_1400 = make_timestamp(2025, 12, 3, 14, 0, 0);
  auto time_1410 = make_timestamp(2025, 12, 3, 14, 10, 0);
  auto time_1420 = make_timestamp(2025, 12, 3, 14, 20, 0);
  auto time_1430 = make_timestamp(2025, 12, 3, 14, 30, 0);

  DataBlock block1 = make_test_block(time_1400, time_1400 + 1000);
  EXPECT_TRUE(builder.write_block(block1));
  std::string path_1400 = builder.get_current_segment_path();
  builder.close_current_segment();

  DataBlock block2 = make_test_block(time_1410, time_1410 + 1000);
  EXPECT_TRUE(builder.write_block(block2));
  std::string path_1410 = builder.get_current_segment_path();
  builder.close_current_segment();

  DataBlock block3 = make_test_block(time_1420, time_1420 + 1000);
  EXPECT_TRUE(builder.write_block(block3));
  std::string path_1420 = builder.get_current_segment_path();
  builder.close_current_segment();

  DataBlock block4 = make_test_block(time_1430, time_1430 + 1000);
  EXPECT_TRUE(builder.write_block(block4));
  std::string path_1430 = builder.get_current_segment_path();
  builder.close_current_segment();

  // 2. 시간 역행 시뮬레이션 (14:30 → 14:00, 30분 역행)
  builder_opts->simulate_time_change(time_1430, time_1400);

  // 3. 역행한 시간으로 블록 쓰기 시도
  // 3-1. 14:00 충돌 (time < old_time) → 무효화
  DataBlock block5 = make_test_block(time_1400, time_1400 + 1000);
  EXPECT_TRUE(builder.write_block(block5));
  builder.close_current_segment();

  EXPECT_EQ(builder_opts->conflict_count, 1);
  EXPECT_TRUE(builder_opts->conflicts[0].should_invalidate);
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 1u);
  EXPECT_TRUE(builder_opts->time_change_detected); // 플래그 유지

  // 3-2. 14:10 충돌 (time < old_time) → 무효화
  DataBlock block6 = make_test_block(time_1410, time_1410 + 1000);
  EXPECT_TRUE(builder.write_block(block6));
  builder.close_current_segment();

  EXPECT_EQ(builder_opts->conflict_count, 2);
  EXPECT_TRUE(builder_opts->conflicts[1].should_invalidate);
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 2u);
  EXPECT_TRUE(builder_opts->time_change_detected); // 플래그 유지

  // 3-3. 14:20 충돌 (time < old_time) → 무효화
  DataBlock block7 = make_test_block(time_1420, time_1420 + 1000);
  EXPECT_TRUE(builder.write_block(block7));
  builder.close_current_segment();

  EXPECT_EQ(builder_opts->conflict_count, 3);
  EXPECT_TRUE(builder_opts->conflicts[2].should_invalidate);
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 3u);
  EXPECT_TRUE(builder_opts->time_change_detected); // 플래그 유지

  // 3-4. 14:30 충돌 (time >= old_time) → 플래그 리셋!
  DataBlock block8 = make_test_block(time_1430, time_1430 + 1000);
  EXPECT_TRUE(builder.write_block(block8));
  builder.close_current_segment();

  EXPECT_EQ(builder_opts->conflict_count, 4);
  EXPECT_FALSE(builder_opts->conflicts[3].should_invalidate); // 무효화 안 함
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 3u);      // 여전히 3개
  EXPECT_FALSE(builder_opts->time_change_detected);           // 플래그 리셋!

  // 4. 이후 충돌은 정상 처리 (무효화 안 함)
  DataBlock block9 = make_test_block(time_1430, time_1430 + 1000);
  EXPECT_TRUE(builder.write_block(block9));

  EXPECT_EQ(builder_opts->conflict_count, 5);
  EXPECT_FALSE(builder_opts->conflicts[4].should_invalidate);
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 3u); // 변화 없음
}

// 테스트 23: 파일 충돌 (시간 변경 없음)
TEST_F(SegmentBuilderTestFixture, FileConflictWithoutTimeChange)
{
  auto builder_opts = std::make_shared<FileConflictTestOptions>(22);
  SegmentBuilder builder(builder_opts);

  // 1. 첫 세그먼트 생성
  auto time1 = make_timestamp(2025, 12, 3, 15, 0, 0);
  DataBlock block1 = make_test_block(time1, time1 + 1000);
  EXPECT_TRUE(builder.write_block(block1));

  std::string first_path = builder.get_current_segment_path();
  builder.close_current_segment();

  // 2. 시간 변경 시뮬레이션 없음

  // 3. 같은 시간으로 재생성 시도 → 충돌
  DataBlock block2 = make_test_block(time1, time1 + 1000);
  EXPECT_TRUE(builder.write_block(block2));

  // 충돌 감지됨
  EXPECT_EQ(builder_opts->conflict_count, 1);
  EXPECT_FALSE(builder_opts->conflicts[0].should_invalidate); // 무효화 안 함
  EXPECT_EQ(builder_opts->invalidated_paths.size(), 0u);

  // 새 세그먼트는 시퀀스 번호로 생성됨
  std::string second_path = builder.get_current_segment_path();
  EXPECT_NE(second_path, first_path);
  EXPECT_TRUE(verify_file_exists(second_path));
}

// 테스트 24: 콜백 예외 처리
TEST_F(SegmentBuilderTestFixture, FileConflictCallbackException)
{
  class ExceptionOptions : public TestSegmentBuilderOptions
  {
  public:
    ExceptionOptions(int64_t ch_id)
        : TestSegmentBuilderOptions(ch_id)
    {
    }

    void on_segment_file_conflict(const std::string&, mstime_t, int) override
    {
      throw std::runtime_error("File conflict callback exception");
    }
  };

  auto builder_opts = std::make_shared<ExceptionOptions>(23);
  SegmentBuilder builder(builder_opts);

  auto time1 = make_timestamp(2025, 12, 3, 16, 0, 0);
  DataBlock block1 = make_test_block(time1, time1 + 1000);
  EXPECT_TRUE(builder.write_block(block1));
  builder.close_current_segment();

  DataBlock block2 = make_test_block(time1, time1 + 1000);

  // 예외가 발생해도 블록 쓰기는 성공해야 함
  EXPECT_NO_THROW({
    auto result = builder.write_block(block2);
    EXPECT_TRUE(result.has_value());
  });

  EXPECT_FALSE(builder.get_current_segment_path().empty());
  EXPECT_TRUE(verify_file_exists(builder.get_current_segment_path()));
}
