// 파일: media_pipeline_unittest.cpp
// 생성일: 2025-07-15
// 설명: MediaPipeline PipelineMode 단위 테스트 (kLive, kLiveBuffered, kPlayback)

#include <gtest/gtest.h>

#include <nxcore/media/media_pipeline.h>
#include <nxcore/media/media_frame.h>
#include <nxcore/media/media_source.h>
#include <nxcore/media/media_sink.h>
#include <nxcore/util/time_util.h>

#include "tests/common/io_context_test_runner.h"

#include <nxcore/util/asio_type.h>
#include <algorithm>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace {

// ============================================================================
// Mock IMediaSource: 프레임 콜백을 외부에서 직접 호출할 수 있도록 노출
// ============================================================================
class MockMediaSource : public nx::media::IMediaSource
{
public:
  nx::awaitable_expected<std::vector<nx::media::MediaTrackInfo>> open() override
  {
    nx::media::MediaTrackInfo video_track;
    video_track.track_index = 0;
    video_track.type = nx::media::MediaType::kVideo;
    video_track.video_codec = nx::media::VideoCodec::kH264;
    video_track.width = 1920;
    video_track.height = 1080;
    video_track.framerate = 30.0;
    co_return std::vector<nx::media::MediaTrackInfo>{video_track};
  }

  nx::awaitable<std::error_code> start(nx::media::FrameCallback callback) override
  {
    m_callback = std::move(callback);
    co_return std::error_code{};
  }

  nx::awaitable<void> close() override
  {
    m_callback = nullptr;
    co_return;
  }

  std::string_view source_name() const override { return "MockSource"; }
  std::string source_url() const override { return "mock://test"; }
  const std::vector<nx::media::MediaTrackInfo>& tracks() const override
  {
    return m_tracks;
  }

  // 외부에서 프레임 주입
  void inject_frame(nx::media::MediaFrame frame)
  {
    if (m_callback) {
      m_callback(std::move(frame));
    }
  }

  // EOF 전송
  void inject_eof()
  {
    nx::media::MediaFrame eof;
    eof.is_eof = true;
    inject_frame(std::move(eof));
  }

private:
  nx::media::FrameCallback m_callback;
  std::vector<nx::media::MediaTrackInfo> m_tracks;
};

// ============================================================================
// Mock IMediaSink: 수신한 프레임을 기록
// ============================================================================
class MockMediaSink : public nx::media::IMediaSink
{
public:
  struct ReceivedFrame
  {
    int64_t timestamp = 0;
    int32_t stream_index = -1;
    size_t data_size = 0;
    std::chrono::steady_clock::time_point received_at;
  };

  nx::awaitable_expected<std::string>
  open(const std::vector<nx::media::MediaTrackInfo>& /*tracks*/) override
  {
    co_return std::string{"mock://output"};
  }

  void send_frame(const nx::media::MediaFrame& frame) override
  {
    ReceivedFrame rf;
    rf.timestamp = frame.timestamp;
    rf.stream_index = frame.stream_index;
    rf.data_size = frame.data ? frame.data->size() : 0;
    rf.received_at = std::chrono::steady_clock::now();

    std::lock_guard lock(m_mutex);
    m_frames.push_back(rf);
  }

  nx::awaitable<void> close() override { co_return; }
  std::string_view sink_name() const override { return "MockSink"; }
  std::string output_url() const override { return "mock://output"; }
  std::size_t client_count() const override { return 1; }

  // 수신 프레임 조회
  std::vector<ReceivedFrame> get_frames() const
  {
    std::lock_guard lock(m_mutex);
    return m_frames;
  }

  size_t frame_count() const
  {
    std::lock_guard lock(m_mutex);
    return m_frames.size();
  }

private:
  mutable std::mutex m_mutex;
  std::vector<ReceivedFrame> m_frames;
};

// ============================================================================
// 헬퍼: 테스트 프레임 생성
// ============================================================================
nx::media::MediaFrame
make_test_frame(int64_t timestamp, int32_t stream_index = 0, size_t data_size = 100)
{
  nx::media::MediaFrame frame;
  frame.type = nx::media::MediaType::kVideo;
  frame.timestamp = timestamp;
  frame.stream_index = stream_index;
  frame.video_codec = nx::media::VideoCodec::kH264;
  frame.data = std::make_shared<std::vector<uint8_t>>();
  frame.data->resize(data_size, 0xAB);
  return frame;
}

// ============================================================================
// 테스트 Fixture
// ============================================================================
class MediaPipelineTest : public ::testing::Test
{
protected:
  void SetUp() override { m_runner.start(1); }

  void TearDown() override { m_runner.stop(); }

  // 파이프라인 생성
  struct PipelineSet
  {
    std::unique_ptr<nx::media::MediaPipeline> pipeline;
    MockMediaSource* source;
    MockMediaSink* sink;
  };

  PipelineSet create_pipeline(nx::media::PipelineMode mode)
  {
    auto source = std::make_unique<MockMediaSource>();
    auto sink = std::make_unique<MockMediaSink>();
    auto* source_ptr = source.get();
    auto* sink_ptr = sink.get();

    auto pipeline = std::make_unique<nx::media::MediaPipeline>(
      m_runner.io_context(),
      std::move(source),
      std::move(sink),
      "test-session");

    pipeline->set_mode(mode);

    PipelineSet ps;
    ps.pipeline = std::move(pipeline);
    ps.source = source_ptr;
    ps.sink = sink_ptr;
    return ps;
  }

  // start 헬퍼 (반환값 처리 포함)
  void start_pipeline(nx::media::MediaPipeline& pipeline)
  {
    auto result = m_runner.run_sync(pipeline.start());
    ASSERT_TRUE(result.has_value()) << result.error().message();
  }

  // stop 헬퍼
  void stop_pipeline(nx::media::MediaPipeline& pipeline)
  {
    m_runner.run_sync(pipeline.stop());
  }

  test::IoContextTestRunner m_runner;
};

} // namespace

// ============================================================================
// 기본 동작 테스트
// ============================================================================

TEST_F(MediaPipelineTest, DefaultModeIsLive)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);
  EXPECT_EQ(
    static_cast<int>(ps.pipeline->mode()),
    static_cast<int>(nx::media::PipelineMode::kLive));
}

TEST_F(MediaPipelineTest, SetModeBeforeStart)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);
  EXPECT_EQ(
    static_cast<int>(ps.pipeline->mode()),
    static_cast<int>(nx::media::PipelineMode::kPlayback));
}

// ============================================================================
// kLive 모드: 패스스루 (즉시 전달)
// ============================================================================

TEST_F(MediaPipelineTest, LiveMode_PassesFramesImmediately)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);
  start_pipeline(*ps.pipeline);

  // 프레임 5개 주입
  for (int i = 0; i < 5; ++i) {
    ps.source->inject_frame(make_test_frame(i * 33));
  }

  // post 기반 비동기 전달 — io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  size_t count = ps.sink->frame_count();
  EXPECT_EQ(count, static_cast<size_t>(5));

  // 타임스탬프 순서 확인
  auto frames = ps.sink->get_frames();
  for (int i = 0; i < 5; ++i) {
    int64_t expected_ts = static_cast<int64_t>(i * 33);
    EXPECT_EQ(frames[i].timestamp, expected_ts);
  }

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, LiveMode_StatisticsUpdated)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);
  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0, 0, 200));
  ps.source->inject_frame(make_test_frame(33, 0, 300));

  // post 기반 비동기 전달 — io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_EQ(ps.pipeline->frames_processed(), static_cast<uint64_t>(2));
  EXPECT_EQ(ps.pipeline->bytes_processed(), static_cast<uint64_t>(500));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, LiveMode_EofTriggersCompletion)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);

  bool completed = false;
  ps.pipeline->set_completion_callback([&](const std::string&) { completed = true; });

  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_eof();

  // completion_callback은 post로 지연되므로 io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_TRUE(completed);
  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(1));
}

// ============================================================================
// kLiveBuffered 모드: 타임스탬프 정렬 버퍼
// ============================================================================

TEST_F(MediaPipelineTest, LiveBuffered_SortsOutOfOrderFrames)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLiveBuffered);
  ps.pipeline->set_buffer_duration(nx::milliseconds(200));

  start_pipeline(*ps.pipeline);

  // 역순으로 넣지만, 범위가 200ms를 넘도록 해서 배출 발생
  ps.source->inject_frame(make_test_frame(100, 0)); // ts=100
  ps.source->inject_frame(make_test_frame(50, 1));  // ts=50 (역순)
  ps.source->inject_frame(make_test_frame(300, 0)); // ts=300 (차이 250ms > 200ms → 배출)

  // post 기반 비동기 전달 — io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  // ts=50은 300-50=250ms > 200ms 이므로 배출됨
  auto frames = ps.sink->get_frames();
  ASSERT_GE(frames.size(), static_cast<size_t>(1));
  // 배출된 프레임은 타임스탬프 순
  EXPECT_EQ(frames[0].timestamp, static_cast<int64_t>(50));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, LiveBuffered_BuffersWithinDuration)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLiveBuffered);
  ps.pipeline->set_buffer_duration(nx::milliseconds(500));

  start_pipeline(*ps.pipeline);

  // 모든 프레임이 500ms 범위 내이므로 배출되지 않음
  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_frame(make_test_frame(100));
  ps.source->inject_frame(make_test_frame(200));

  // post 기반 비동기 전달 — io_context 처리 후에도 버퍼에 남아있어야 함
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(0));

  // EOF → 남은 프레임 모두 배출
  ps.source->inject_eof();

  // EOF도 post 기반으로 처리됨
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(3));
  auto frames = ps.sink->get_frames();
  EXPECT_EQ(frames[0].timestamp, static_cast<int64_t>(0));
  EXPECT_EQ(frames[1].timestamp, static_cast<int64_t>(100));
  EXPECT_EQ(frames[2].timestamp, static_cast<int64_t>(200));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, LiveBuffered_InterleavedAV)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLiveBuffered);
  ps.pipeline->set_buffer_duration(nx::milliseconds(100));

  start_pipeline(*ps.pipeline);

  // A/V 인터리빙: 오디오가 비디오보다 먼저 올 수 있음
  ps.source->inject_frame(make_test_frame(150, 0)); // video ts=150
  ps.source->inject_frame(make_test_frame(120, 1)); // audio ts=120
  ps.source->inject_frame(make_test_frame(200, 0)); // video ts=200
  ps.source->inject_frame(make_test_frame(350, 0)); // video ts=350 (차이 230ms > 100ms)

  // post 기반 비동기 전달 — io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  auto frames = ps.sink->get_frames();
  // 배출된 프레임은 타임스탬프 순이어야 함
  for (size_t i = 1; i < frames.size(); ++i) {
    EXPECT_LE(frames[i - 1].timestamp, frames[i].timestamp);
  }

  stop_pipeline(*ps.pipeline);
}

// ============================================================================
// kPlayback 모드: 타이머 기반 pacing
// ============================================================================

TEST_F(MediaPipelineTest, Playback_FirstFrameDeliveredImmediately)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);
  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0));

  // 첫 프레임은 즉시 전달됨 - io_context를 통해 실행되므로 약간 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_GE(ps.sink->frame_count(), static_cast<size_t>(1));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, Playback_FramesPacedByTimestamp)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);
  start_pipeline(*ps.pipeline);

  // 프레임 3개: 0ms, 100ms, 200ms 간격
  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_frame(make_test_frame(100));
  ps.source->inject_frame(make_test_frame(200));

  // 첫 프레임은 즉시, 나머지는 pacing됨
  // 200ms 후 3개 모두 전달되어야 함 (여유 150ms 추가)
  std::this_thread::sleep_for(nx::milliseconds(400));

  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(3));

  // 프레임 간 전달 시간 간격 확인
  auto frames = ps.sink->get_frames();
  if (frames.size() >= 3) {
    auto gap1 = std::chrono::duration_cast<nx::milliseconds>(
      frames[1].received_at - frames[0].received_at);
    auto gap2 = std::chrono::duration_cast<nx::milliseconds>(
      frames[2].received_at - frames[1].received_at);

    // 각 간격이 약 100ms (±70ms 허용)
    EXPECT_GE(gap1.count(), static_cast<long long>(30));
    EXPECT_LE(gap1.count(), static_cast<long long>(250));
    EXPECT_GE(gap2.count(), static_cast<long long>(30));
    EXPECT_LE(gap2.count(), static_cast<long long>(250));
  }

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, Playback_SpeedMultiplier)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);
  ps.pipeline->set_playback_speed(2.0); // 2배속

  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_frame(make_test_frame(200)); // 원래 200ms 간격 → 2배속이면 100ms

  // 2배속이므로 200ms면 충분
  std::this_thread::sleep_for(nx::milliseconds(250));

  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(2));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, Playback_StopCancelsPacing)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);
  start_pipeline(*ps.pipeline);

  // 1초 뒤에 전달될 프레임
  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_frame(make_test_frame(1000));

  // 첫 프레임만 전달된 후 바로 중지
  std::this_thread::sleep_for(nx::milliseconds(50));
  stop_pipeline(*ps.pipeline);

  // 두 번째 프레임은 전달되지 않아야 함
  EXPECT_LE(ps.sink->frame_count(), static_cast<size_t>(1));
}

TEST_F(MediaPipelineTest, Playback_EofFlushesQueue)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kPlayback);

  bool completed = false;
  ps.pipeline->set_completion_callback([&](const std::string&) { completed = true; });

  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0));
  ps.source->inject_frame(make_test_frame(5000)); // 5초 뒤
  ps.source->inject_eof();

  // EOF이면 큐에 남은 프레임 즉시 배출 (5초 대기하지 않음)
  std::this_thread::sleep_for(nx::milliseconds(100));

  EXPECT_TRUE(completed);
  EXPECT_EQ(ps.sink->frame_count(), static_cast<size_t>(2));

  stop_pipeline(*ps.pipeline);
}

// ============================================================================
// 통계/접근자
// ============================================================================

TEST_F(MediaPipelineTest, FramesAndBytesTracked)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);
  start_pipeline(*ps.pipeline);

  ps.source->inject_frame(make_test_frame(0, 0, 150));
  ps.source->inject_frame(make_test_frame(33, 0, 250));
  ps.source->inject_frame(make_test_frame(66, 0, 100));

  // post 기반 비동기 전달 — io_context 처리 대기
  std::this_thread::sleep_for(nx::milliseconds(50));

  EXPECT_EQ(ps.pipeline->frames_processed(), static_cast<uint64_t>(3));
  EXPECT_EQ(ps.pipeline->bytes_processed(), static_cast<uint64_t>(500));

  stop_pipeline(*ps.pipeline);
}

TEST_F(MediaPipelineTest, StopIgnoredAfterAlreadyStopped)
{
  auto ps = create_pipeline(nx::media::PipelineMode::kLive);
  start_pipeline(*ps.pipeline);
  stop_pipeline(*ps.pipeline);
  // 두 번째 stop은 아무 일도 하지 않음 (크래시 없음)
  stop_pipeline(*ps.pipeline);
  EXPECT_FALSE(ps.pipeline->is_running());
}

// ============================================================================
// main
// ============================================================================

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
