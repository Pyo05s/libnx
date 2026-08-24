// 파일: multi_sink_unittest.cpp
// 생성일: 2026-03-11
// 설명: MultiSink 단위 테스트

#include <gtest/gtest.h>

#include "nxcore/media/multi_sink.h"
#include "nxcore/media/media_frame.h"
#include "nxcore/media/media_sink.h"

#include "tests/common/io_context_test_runner.h"
#include "tests/common/coroutine_helper.h"

#include <nxcore/util/asio_type.h>
#include <atomic>
#include <mutex>
#include <vector>

namespace {

// ============================================================================
// 헬퍼: 트랙 정보 생성
// ============================================================================
nx::media::MediaTrackInfo
make_video_track(int index = 0)
{
  nx::media::MediaTrackInfo t;
  t.track_index = index;
  t.type = nx::media::MediaType::kVideo;
  t.video_codec = nx::media::VideoCodec::kH264;
  t.width = 1920;
  t.height = 1080;
  t.framerate = 30.0;
  return t;
}

nx::media::MediaFrame
make_video_frame(int64_t timestamp = 100)
{
  nx::media::MediaFrame frame;
  frame.type = nx::media::MediaType::kVideo;
  frame.timestamp = timestamp;
  frame.stream_index = 0;
  frame.video_codec = nx::media::VideoCodec::kH264;
  frame.data = std::make_shared<std::vector<uint8_t>>();
  frame.data->resize(128, 0xAB);
  return frame;
}

// ============================================================================
// MockSink: 수신 이력과 open/close 상태를 추적
// ============================================================================
class MockSink : public nx::media::IMediaSink
{
public:
  explicit MockSink(std::string url = "mock://output", std::size_t client_cnt = 1)
      : m_url(std::move(url))
      , m_client_count(client_cnt)
  {}

  nx::awaitable_expected<std::string>
  open(const std::vector<nx::media::MediaTrackInfo>& tracks) override
  {
    if (m_inject_open_error) {
      co_return std::unexpected(m_injected_error);
    }
    m_opened = true;
    m_open_track_count = static_cast<int>(tracks.size());
    co_return m_url;
  }

  void send_frame(const nx::media::MediaFrame& frame) override
  {
    std::lock_guard lock(m_mutex);
    m_timestamps.push_back(frame.timestamp);
  }

  nx::awaitable<void> close() override
  {
    m_opened = false;
    ++m_close_count;
    co_return;
  }

  std::string_view sink_name() const override { return "MockSink"; }
  std::string output_url() const override { return m_url; }
  std::size_t client_count() const override { return m_client_count; }

  // 테스트 헬퍼
  void set_open_error(std::error_code ec)
  {
    m_inject_open_error = true;
    m_injected_error = ec;
  }

  bool is_opened() const { return m_opened; }
  int open_track_count() const { return m_open_track_count; }
  int close_count() const { return m_close_count; }

  std::vector<int64_t> received_timestamps() const
  {
    std::lock_guard lock(m_mutex);
    return m_timestamps;
  }

private:
  std::string m_url;
  std::size_t m_client_count;
  std::atomic<bool> m_opened{false};
  std::atomic<int> m_open_track_count{0};
  std::atomic<int> m_close_count{0};

  bool m_inject_open_error{false};
  std::error_code m_injected_error;

  mutable std::mutex m_mutex;
  std::vector<int64_t> m_timestamps;
};

} // namespace

// ============================================================================
// 테스트 Fixture
// ============================================================================
class MultiSinkTest : public ::testing::Test
{
protected:
  void SetUp() override { m_runner.start(); }

  test::IoContextTestRunner m_runner;
};

// ============================================================================
// 1. open/close 기본 동작
// ============================================================================

TEST_F(MultiSinkTest, Open_EmptySinks_Succeeds)
{
  // 싱크 없이 open해도 성공해야 한다
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    auto result = co_await multi.open({make_video_track()});
    CO_ASSERT_TRUE(result.has_value());
  }());
}

TEST_F(MultiSinkTest, Open_SingleSink_OpensChild)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto sink = std::make_unique<MockSink>("mock://video");
    auto* raw = sink.get();

    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::move(sink));
    auto result = co_await multi.open({make_video_track()});

    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(*result, std::string("mock://video"));
    CO_ASSERT_TRUE(raw->is_opened());
    CO_ASSERT_EQ(raw->open_track_count(), 1);
  }());
}

TEST_F(MultiSinkTest, Close_CallsAllChildren)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto sink1 = std::make_unique<MockSink>();
    auto sink2 = std::make_unique<MockSink>();
    auto* raw1 = sink1.get();
    auto* raw2 = sink2.get();

    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::move(sink1));
    std::ignore = co_await multi.add_sink(std::move(sink2));
    std::ignore = co_await multi.open({make_video_track()});
    co_await multi.close();

    CO_ASSERT_FALSE(raw1->is_opened());
    CO_ASSERT_FALSE(raw2->is_opened());
  }());
}

// ============================================================================
// 2. send_frame 팬아웃
// ============================================================================

TEST_F(MultiSinkTest, SendFrame_FansOutToAllSinks)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto sink1 = std::make_unique<MockSink>();
    auto sink2 = std::make_unique<MockSink>();
    auto* raw1 = sink1.get();
    auto* raw2 = sink2.get();

    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::move(sink1));
    std::ignore = co_await multi.add_sink(std::move(sink2));
    std::ignore = co_await multi.open({make_video_track()});

    multi.send_frame(make_video_frame(100));
    multi.send_frame(make_video_frame(200));

    CO_ASSERT_EQ(raw1->received_timestamps(), (std::vector<int64_t>{100, 200}));
    CO_ASSERT_EQ(raw2->received_timestamps(), (std::vector<int64_t>{100, 200}));
  }());
}

// ============================================================================
// 3. 동적 add_sink / remove_sink
// ============================================================================

TEST_F(MultiSinkTest, AddSink_BeforeOpen_DoesNotOpenChild)
{
  // open 전에 추가한 싱크는 open() 호출 때까지 열리지 않아야 한다
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto sink = std::make_unique<MockSink>();
    auto* raw = sink.get();

    nx::media::MultiSink multi;
    auto id_result = co_await multi.add_sink(std::move(sink));

    CO_ASSERT_TRUE(id_result.has_value());
    CO_ASSERT_FALSE(raw->is_opened()); // 아직 열리지 않음
  }());
}

TEST_F(MultiSinkTest, AddSink_AfterOpen_OpensChildImmediately)
{
  // open 후에 추가한 싱크는 즉시 open() 되어야 한다
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    std::ignore = co_await multi.open({make_video_track()});

    auto sink = std::make_unique<MockSink>();
    auto* raw = sink.get();
    auto id_result = co_await multi.add_sink(std::move(sink));

    CO_ASSERT_TRUE(id_result.has_value());
    CO_ASSERT_TRUE(raw->is_opened());
    CO_ASSERT_EQ(raw->open_track_count(), 1);
  }());
}

TEST_F(MultiSinkTest, RemoveSink_ClosesAndStopsReceivingFrames)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto sink = std::make_unique<MockSink>();
    auto* raw = sink.get();

    nx::media::MultiSink multi;
    auto id_result = co_await multi.add_sink(std::move(sink));
    CO_ASSERT_TRUE(id_result.has_value());

    std::ignore = co_await multi.open({make_video_track()});

    // 제거 전: 프레임 전달 확인 (raw 유효)
    multi.send_frame(make_video_frame(100));
    CO_ASSERT_EQ(raw->received_timestamps().size(), static_cast<std::size_t>(1));
    CO_ASSERT_EQ(raw->close_count(), 0);

    // remove_sink 이후 raw는 dangling → 접근 금지
    co_await multi.remove_sink(*id_result);

    // 제거 후: 프레임이 더 이상 전달되지 않아야 함
    multi.send_frame(make_video_frame(200));
    CO_ASSERT_EQ(multi.client_count(), static_cast<std::size_t>(0));
  }());
}

TEST_F(MultiSinkTest, AddSink_ReturnsUniqueIds)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;

    auto r1 = co_await multi.add_sink(std::make_unique<MockSink>());
    auto r2 = co_await multi.add_sink(std::make_unique<MockSink>());

    CO_ASSERT_TRUE(r1.has_value());
    CO_ASSERT_TRUE(r2.has_value());
    CO_ASSERT_NE(*r1, *r2);
  }());
}

// ============================================================================
// 4. client_count 집계
// ============================================================================

TEST_F(MultiSinkTest, ClientCount_SumsAllChildren)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::make_unique<MockSink>("u1", 3));
    std::ignore = co_await multi.add_sink(std::make_unique<MockSink>("u2", 5));
    std::ignore = co_await multi.open({make_video_track()});

    CO_ASSERT_EQ(multi.client_count(), static_cast<std::size_t>(8));
  }());
}

TEST_F(MultiSinkTest, ClientCount_ZeroWhenEmpty)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    CO_ASSERT_EQ(multi.client_count(), static_cast<std::size_t>(0));
  }());
}

// ============================================================================
// 5. sink_name / output_url
// ============================================================================

TEST_F(MultiSinkTest, SinkName_IsMultiSink)
{
  nx::media::MultiSink multi;
  EXPECT_EQ(multi.sink_name(), "MultiSink");
}

TEST_F(MultiSinkTest, OutputUrl_ReturnsFirstSinkUrl)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::make_unique<MockSink>("mock://first"));
    std::ignore = co_await multi.add_sink(std::make_unique<MockSink>("mock://second"));
    std::ignore = co_await multi.open({make_video_track()});

    CO_ASSERT_EQ(multi.output_url(), std::string("mock://first"));
  }());
}

// ============================================================================
// 6. 에러 처리
// ============================================================================

TEST_F(MultiSinkTest, Open_ChildOpenFails_ReturnsError)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    auto failing_sink = std::make_unique<MockSink>();
    failing_sink->set_open_error(std::make_error_code(std::errc::io_error));

    nx::media::MultiSink multi;
    std::ignore = co_await multi.add_sink(std::move(failing_sink));
    auto result = co_await multi.open({make_video_track()});

    CO_ASSERT_FALSE(result.has_value());
  }());
}

TEST_F(MultiSinkTest, AddSink_AfterOpen_ChildOpenFails_ReturnsError)
{
  m_runner.run_sync([&]() -> nx::awaitable<void> {
    nx::media::MultiSink multi;
    std::ignore = co_await multi.open({make_video_track()});

    auto failing_sink = std::make_unique<MockSink>();
    failing_sink->set_open_error(std::make_error_code(std::errc::io_error));
    auto result = co_await multi.add_sink(std::move(failing_sink));

    CO_ASSERT_FALSE(result.has_value());
  }());
}
