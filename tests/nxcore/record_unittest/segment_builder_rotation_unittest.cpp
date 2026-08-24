// 파일: segment_builder_rotation_unittest.cpp
// 생성일: 2025-01-15
// 설명: SegmentBuilder 세그먼트 전환 테스트

#include "segment_builder_test_fixture.h"

// 테스트 3: 파일 크기 제한에 의한 세그먼트 전환
TEST_F(SegmentBuilderTestFixture, SegmentRotationByFileSize)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(3);
  builder_opts->set_max_file_size(500);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 9, 0, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 10; ++i) {
    DataBlock block
      = make_test_block(base_ts + (i * 1000), base_ts + ((i + 1) * 1000), 200);
    EXPECT_TRUE(builder.write_block(block));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  EXPECT_GT(segment_paths.size(), 1u);

  for (const auto& path : segment_paths) {
    EXPECT_TRUE(verify_file_exists(path));
  }

  if (segment_paths.size() > 1) {
    for (std::size_t i = 1; i < segment_paths.size(); ++i) {
      std::string path = segment_paths[i];
      EXPECT_NE(path.find("_00"), std::string::npos)
        << "세그먼트 파일에 시퀀스 번호가 없습니다: " << path;
    }
  }
}

// 테스트 4: 시간 제한에 의한 세그먼트 전환
TEST_F(SegmentBuilderTestFixture, SegmentRotationByDuration)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(4);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(120 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 11, 0, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 3; ++i) {
    mstime_t ts = base_ts + (i * 120 * 1000);
    DataBlock block = make_test_block(ts, ts + 1000, 50);
    EXPECT_TRUE(builder.write_block(block));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  EXPECT_GT(segment_paths.size(), 1u);

  for (const auto& path : segment_paths) {
    EXPECT_TRUE(verify_file_exists(path));

    std::string generic = fs::path(path).generic_string();
    std::size_t seq_pos = generic.find("_00");
    std::size_t ext_pos = generic.rfind(".nxb");

    if (seq_pos != std::string::npos && ext_pos != std::string::npos) {
      EXPECT_GT(ext_pos - seq_pos, 4u)
        << "1분 이상 시간 기반 분할에서는 시퀀스 번호가 없어야 합니다: " << generic;
    }
  }
}

// 테스트 10: 시퀀스 번호 기능 확인
TEST_F(SegmentBuilderTestFixture, SequenceNumbering)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(9);
  builder_opts->set_max_file_size(300);
  builder_opts->set_max_duration(3600 * 1000);
  SegmentBuilder builder(builder_opts);

  auto timestamp = make_timestamp(2025, 12, 1, 18, 30, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 5; ++i) {
    DataBlock block
      = make_test_block(timestamp + (i * 1000), timestamp + ((i + 1) * 1000), 150);
    EXPECT_TRUE(builder.write_block(block));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  ASSERT_GE(segment_paths.size(), 2u);

  std::string first_path = segment_paths[0];
  EXPECT_TRUE(verify_file_exists(first_path));

  std::size_t seq_pos = first_path.find("_00");
  std::size_t ext_pos = first_path.rfind(".nxb");
  if (seq_pos != std::string::npos && ext_pos != std::string::npos) {
    EXPECT_GT(ext_pos - seq_pos, 4u)
      << "첫 번째 파일에는 시퀀스 번호가 없어야 합니다: " << first_path;
  }

  for (std::size_t i = 1; i < segment_paths.size(); ++i) {
    std::string path = segment_paths[i];
    EXPECT_TRUE(verify_file_exists(path));

    char expected_seq[8];
    std::snprintf(expected_seq, sizeof(expected_seq), "_%03zu", i);

    EXPECT_NE(path.find(expected_seq), std::string::npos)
      << "파일 " << i << "에 올바른 시퀀스 번호가 없습니다. 기대값: " << expected_seq
      << ", 실제: " << path;
  }

  for (std::size_t i = 0; i < segment_paths.size(); ++i) {
    for (std::size_t j = i + 1; j < segment_paths.size(); ++j) {
      EXPECT_NE(segment_paths[i], segment_paths[j])
        << "중복된 파일 경로: " << segment_paths[i];
    }
  }
}

// 테스트 14: 1분 미만 시간 기반 세그먼트
TEST_F(SegmentBuilderTestFixture, ShortDurationSegmentSequencing)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(13);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(10 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 1, 23, 30, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 4; ++i) {
    mstime_t ts = base_ts + (i * 10 * 1000);
    DataBlock block = make_test_block(ts, ts + 1000, 50);
    EXPECT_TRUE(builder.write_block(block));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  builder.close_current_segment();
  EXPECT_GT(segment_paths.size(), 1u);

  if (segment_paths.size() > 1) {
    for (std::size_t i = 1; i < segment_paths.size(); ++i) {
      std::string path = segment_paths[i];
      std::string generic = fs::path(path).generic_string();

      EXPECT_NE(generic.find("_00"), std::string::npos)
        << "1분 미만 세그먼트에는 시퀀스 번호가 있어야 합니다: " << generic;
    }
  }

  for (const auto& path : segment_paths) {
    EXPECT_TRUE(verify_file_exists(path));
  }
}

// 테스트 15: 1분 이상 시간 기반 세그먼트
TEST_F(SegmentBuilderTestFixture, LongDurationSegmentNoSequencing)
{
  auto builder_opts = std::make_shared<TestSegmentBuilderOptions>(14);
  builder_opts->set_max_file_size(1024 * 1024);
  builder_opts->set_max_duration(120 * 1000);
  SegmentBuilder builder(builder_opts);

  auto base_ts = make_timestamp(2025, 12, 2, 0, 0, 0);
  std::vector<std::string> segment_paths;

  for (int i = 0; i < 3; ++i) {
    mstime_t ts = base_ts + (i * 120 * 1000);
    DataBlock block = make_test_block(ts, ts + 1000, 50);
    EXPECT_TRUE(builder.write_block(block));

    std::string current_path = builder.get_current_segment_path();
    if (segment_paths.empty() || segment_paths.back() != current_path) {
      segment_paths.push_back(current_path);
    }
  }

  builder.close_current_segment();
  EXPECT_GT(segment_paths.size(), 1u);

  for (const auto& path : segment_paths) {
    EXPECT_TRUE(verify_file_exists(path));

    std::string generic = fs::path(path).generic_string();
    std::size_t seq_pos = generic.find("_00");
    std::size_t ext_pos = generic.rfind(".nxb");

    if (seq_pos != std::string::npos && ext_pos != std::string::npos) {
      EXPECT_GT(ext_pos - seq_pos, 4u)
        << "1분 이상 시간 기반 세그먼트에는 시퀀스 번호가 있어야 합니다: " << generic;
    }
  }
}
