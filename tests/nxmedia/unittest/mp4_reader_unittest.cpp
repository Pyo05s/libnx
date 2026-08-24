// 파일: mp4_reader_unittest.cpp
// 생성일: 2026-02-06
// 설명: Mp4Reader 단위 테스트

#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <iostream>

#include "nxcore/media/media_type.h"
#include "nxmedia/file/mp4_reader.h"

namespace fs = std::filesystem;

namespace {

// 테스트용 임시 디렉터리
const fs::path kTestSourceDir = fs::path(__FILE__).parent_path();
const fs::path kTestDataDir =
  (kTestSourceDir / "../../temp/mp4_reader_test").lexically_normal();

// 테스트용 샘플 미디어 파일 경로
const fs::path kSampleMp4File =
  (kTestSourceDir / "../../data/media/perfect_night_5sec.mp4").lexically_normal();
const fs::path kSampleH265Mp4File =
  (kTestSourceDir / "../../data/media/perfect_night_5sec_h265.mp4").lexically_normal();

bool
has_annex_b_start_code(const std::vector<uint8_t>& data)
{
  if (data.size() < 4) {
    return false;
  }

  const bool has_four_byte_start_code =
    data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00 && data[3] == 0x01;
  const bool has_three_byte_start_code =
    data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01;

  return has_four_byte_start_code || has_three_byte_start_code;
}

void
expect_first_video_frame_as_annex_b(const fs::path& filepath)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(filepath);
  ASSERT_FALSE(ec);

  bool found_video = false;

  while (auto result = reader.read_frame()) {
    const auto& frame = result.value();

    if (frame.type != nx::media::MediaType::kVideo) {
      continue;
    }

    found_video = true;
    EXPECT_TRUE(has_annex_b_start_code(frame.data));
    break;
  }

  EXPECT_TRUE(found_video);
}

} // anonymous namespace

class Mp4ReaderTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 테스트 디렉터리 생성
    std::error_code ec;
    fs::create_directories(kTestDataDir, ec);
  }

  void TearDown() override
  {
    // 테스트 디렉터리 정리
    std::error_code ec;
    fs::remove_all(kTestDataDir, ec);
  }

  // 더미 MP4 파일 생성 (실제 테스트에서는 유효한 샘플 파일 필요)
  fs::path create_dummy_file(const std::string& filename)
  {
    fs::path filepath = kTestDataDir / filename;
    std::ofstream file(filepath, std::ios::binary);
    file << "dummy";
    return filepath;
  }
};

// 기본 기능 테스트
TEST_F(Mp4ReaderTest, ConstructorAndDestructor)
{
  nx::media::Mp4Reader reader;
  EXPECT_FALSE(reader.is_open());
}

TEST_F(Mp4ReaderTest, OpenNonExistentFile)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open("non_existent_file.mp4");

  EXPECT_TRUE(ec);
  EXPECT_FALSE(reader.is_open());
}

TEST_F(Mp4ReaderTest, OpenInvalidFile)
{
  auto filepath = create_dummy_file("invalid.mp4");

  nx::media::Mp4Reader reader;
  auto ec = reader.open(filepath);

  EXPECT_TRUE(ec);
  EXPECT_FALSE(reader.is_open());
}

TEST_F(Mp4ReaderTest, GetMediaInfoBeforeOpen)
{
  nx::media::Mp4Reader reader;
  const auto& info = reader.get_media_info();

  EXPECT_EQ(info.duration_ms, 0);
  EXPECT_EQ(info.bitrate, 0);
  EXPECT_TRUE(info.streams.empty());
}

TEST_F(Mp4ReaderTest, ReadFrameBeforeOpen)
{
  nx::media::Mp4Reader reader;
  auto result = reader.read_frame();

  EXPECT_FALSE(result.has_value());
  EXPECT_TRUE(result.error());
}

TEST_F(Mp4ReaderTest, SeekBeforeOpen)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.seek(1000);

  EXPECT_TRUE(ec);
}

TEST_F(Mp4ReaderTest, CloseWithoutOpen)
{
  nx::media::Mp4Reader reader;

  // 예외 발생하지 않아야 함
  EXPECT_NO_THROW(reader.close());
  EXPECT_FALSE(reader.is_open());
}

TEST_F(Mp4ReaderTest, MultipleClose)
{
  nx::media::Mp4Reader reader;

  // 여러 번 close 호출해도 안전해야 함
  EXPECT_NO_THROW({
    reader.close();
    reader.close();
    reader.close();
  });
}

TEST_F(Mp4ReaderTest, MoveConstructor)
{
  nx::media::Mp4Reader reader1;
  nx::media::Mp4Reader reader2(std::move(reader1));

  // 이동 후 reader2가 사용 가능해야 함
  EXPECT_FALSE(reader2.is_open());
}

TEST_F(Mp4ReaderTest, MoveAssignment)
{
  nx::media::Mp4Reader reader1;
  nx::media::Mp4Reader reader2;

  reader2 = std::move(reader1);

  // 이동 후 reader2가 사용 가능해야 함
  EXPECT_FALSE(reader2.is_open());
}

// 유효한 MP4 파일을 사용한 통합 테스트
TEST_F(Mp4ReaderTest, OpenValidFile)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);

  EXPECT_FALSE(ec) << "Failed to open file: " << ec.message();
  EXPECT_TRUE(reader.is_open());

  // 미디어 정보 확인
  const auto& info = reader.get_media_info();
  EXPECT_GT(info.duration_ms, 0);
  EXPECT_FALSE(info.streams.empty());
}

TEST_F(Mp4ReaderTest, GetStreamInfo)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  const auto& info = reader.get_media_info();
  ASSERT_FALSE(info.streams.empty());

  // 스트림 정보 출력 (디버깅용)
  std::cout << "Media duration: " << info.duration_ms << "ms, " << info.streams.size()
            << " streams" << std::endl;

  bool has_video = false;
  bool has_audio = false;

  for (const auto& stream : info.streams) {
    EXPECT_GE(stream.index, 0);
    EXPECT_NE(stream.type, nx::media::MediaType::kUnknown);
    EXPECT_FALSE(stream.codec_name.empty());

    std::cout << "Stream " << stream.index << ": type=" << static_cast<int>(stream.type)
              << ", codec=" << stream.codec_name << ", bitrate=" << stream.bitrate
              << std::endl;

    // 비디오 스트림이면 해상도와 FPS 확인
    if (stream.type == nx::media::MediaType::kVideo) {
      EXPECT_GT(stream.width, 0);
      EXPECT_GT(stream.height, 0);
      EXPECT_GT(stream.fps, 0.0);
      has_video = true;

      std::cout << "  Video: " << stream.width << "x" << stream.height << " @ "
                << stream.fps << " fps" << std::endl;
    }
    // 오디오 스트림이면 샘플레이트와 채널 확인
    else if (stream.type == nx::media::MediaType::kAudio) {
      EXPECT_GT(stream.sample_rate, 0);
      EXPECT_GT(stream.channels, 0);
      has_audio = true;

      std::cout << "  Audio: " << stream.sample_rate << " Hz, " << stream.channels
                << " channels" << std::endl;
    }
  }

  // 최소한 비디오 또는 오디오 스트림이 있어야 함
  EXPECT_TRUE(has_video || has_audio);
}

TEST_F(Mp4ReaderTest, ReadFirstFrame)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  auto result = reader.read_frame();
  ASSERT_TRUE(result.has_value()) << "Failed to read frame: " << result.error().message();

  const auto& frame = result.value();
  EXPECT_TRUE(frame.encoded);
  EXPECT_GE(frame.stream_index, 0);
  EXPECT_FALSE(frame.data.empty());

  std::cout << "First frame: type=" << static_cast<int>(frame.type)
            << ", timestamp=" << frame.timestamp << "ms"
            << ", size=" << frame.data.size() << " bytes"
            << ", keyframe=" << frame.is_keyframe << std::endl;
}

TEST_F(Mp4ReaderTest, ReadVideoFrameAsAnnexB)
{
  expect_first_video_frame_as_annex_b(kSampleMp4File);
}

TEST_F(Mp4ReaderTest, ReadH265VideoFrameAsAnnexB)
{
  expect_first_video_frame_as_annex_b(kSampleH265Mp4File);
}

TEST_F(Mp4ReaderTest, ReadAllFrames)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  int frame_count = 0;
  int video_frames = 0;
  int audio_frames = 0;
  int keyframe_count = 0;

  while (auto result = reader.read_frame()) {
    const auto& frame = result.value();
    EXPECT_FALSE(frame.data.empty());
    EXPECT_GE(frame.timestamp, 0);
    EXPECT_TRUE(frame.encoded);

    if (frame.type == nx::media::MediaType::kVideo) {
      video_frames++;
      if (frame.is_keyframe) {
        keyframe_count++;
      }
    }
    else if (frame.type == nx::media::MediaType::kAudio) {
      audio_frames++;
    }

    frame_count++;
  }

  EXPECT_GT(frame_count, 0);
  std::cout << "Total frames: " << frame_count << ", Video: " << video_frames
            << ", Audio: " << audio_frames << ", Keyframes: " << keyframe_count
            << std::endl;

  // 비디오 프레임이 있다면 최소한 하나의 키프레임이 있어야 함
  if (video_frames > 0) {
    EXPECT_GT(keyframe_count, 0);
  }
}

TEST_F(Mp4ReaderTest, SeekToBeginning)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  // 몇 프레임 읽기
  for (int i = 0; i < 10; i++) {
    auto result = reader.read_frame();
    if (!result.has_value())
      break;
  }

  // 처음으로 돌아가기
  ec = reader.seek(0);
  EXPECT_FALSE(ec) << "Seek failed: " << ec.message();

  // 첫 프레임 다시 읽기
  auto result = reader.read_frame();
  ASSERT_TRUE(result.has_value());

  const auto& frame = result.value();
  EXPECT_EQ(frame.timestamp, 0);
}

TEST_F(Mp4ReaderTest, SeekToMiddle)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  const auto& info = reader.get_media_info();
  int64_t middle_time = info.duration_ms / 2;

  // 중간 지점으로 이동
  ec = reader.seek(middle_time);
  EXPECT_FALSE(ec) << "Seek to middle failed: " << ec.message();

  // 이동 후 프레임 읽기
  auto result = reader.read_frame();
  ASSERT_TRUE(result.has_value());

  const auto& frame = result.value();
  // seek는 가장 가까운 이전 키프레임으로 이동하므로 정확하지 않을 수 있음
  // 최소한 0보다는 커야 하고, 파일 끝보다는 작아야 함
  EXPECT_GT(frame.timestamp, 0);
  EXPECT_LT(frame.timestamp, info.duration_ms);

  std::cout << "Seek to " << middle_time << "ms, got frame at " << frame.timestamp << "ms"
            << std::endl;
}

TEST_F(Mp4ReaderTest, MultipleSeeks)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  const auto& info = reader.get_media_info();

  // 여러 위치로 이동
  std::vector<int64_t> positions = {0, 1000, 2000, 3000, 0};

  for (auto pos : positions) {
    // duration을 넘지 않도록 조정
    int64_t seek_pos = std::min(pos, info.duration_ms - 100);

    ec = reader.seek(seek_pos);
    EXPECT_FALSE(ec) << "Seek to " << seek_pos << "ms failed: " << ec.message();

    auto result = reader.read_frame();
    EXPECT_TRUE(result.has_value());
  }
}

TEST_F(Mp4ReaderTest, CloseAndReopen)
{
  nx::media::Mp4Reader reader;

  // 처음 열기
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);
  EXPECT_TRUE(reader.is_open());

  // 프레임 읽기
  auto result = reader.read_frame();
  EXPECT_TRUE(result.has_value());

  // 닫기
  reader.close();
  EXPECT_FALSE(reader.is_open());

  // 다시 열기
  ec = reader.open(kSampleMp4File);
  EXPECT_FALSE(ec);
  EXPECT_TRUE(reader.is_open());

  // 다시 프레임 읽기
  result = reader.read_frame();
  EXPECT_TRUE(result.has_value());
}

TEST_F(Mp4ReaderTest, ReadAfterClose)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  reader.close();

  // 닫은 후 읽기 시도
  auto result = reader.read_frame();
  EXPECT_FALSE(result.has_value());
  EXPECT_TRUE(result.error());
}

TEST_F(Mp4ReaderTest, SeekAfterClose)
{
  nx::media::Mp4Reader reader;
  auto ec = reader.open(kSampleMp4File);
  ASSERT_FALSE(ec);

  reader.close();

  // 닫은 후 seek 시도
  ec = reader.seek(1000);
  EXPECT_TRUE(ec);
}
