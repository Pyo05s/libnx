// 파일: rtsp_client_integration_test.cpp
// 생성일: 2026-02-23
// 설명: RTSP 클라이언트 통합 테스트 (실제 카메라 연결)

#include <gtest/gtest.h>
#include <nxcore/util/time_util.h>
#include <nxnet/rtsp/rtsp_client.h>
#include <nxnet/sdp/sdp_session.h>

#include "tests/common/io_context_test_runner.h"
#include "tests/common/coroutine_helper.h"

#include <spdlog/spdlog.h>
#include <atomic>

// ============================================================================
// 실제 카메라 RTSP 통합 테스트
// ============================================================================

class RtspClientIntegrationTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_runner = std::make_unique<test::IoContextTestRunner>();
    m_runner->start(2);
  }

  void TearDown() override { m_runner->stop(); }

  // 테스트 카메라 정보
  static constexpr const char* kCameraIp = "192.168.0.168";
  static constexpr uint16_t kCameraPort = 554;
  static constexpr const char* kUsername = "admin";
  static constexpr const char* kPassword = "cctv3200*";
  static constexpr const char* kRtspUrl1 = "rtsp://192.168.0.168/media/video1";
  static constexpr const char* kRtspUrl2 = "rtsp://192.168.0.168/media/video2";
  static constexpr const char* kRtspUrl3 = "rtsp://192.168.0.168/media/video3";

  std::unique_ptr<test::IoContextTestRunner> m_runner;
};

// 연결 테스트
TEST_F(RtspClientIntegrationTest, ConnectToCamera)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);
    EXPECT_TRUE(client.is_connected());

    co_await client.close();
    EXPECT_FALSE(client.is_connected());
  };

  m_runner->run_sync(test_logic(), nx::seconds(10));
}

// DESCRIBE 테스트
TEST_F(RtspClientIntegrationTest, DescribeStream)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);

    auto sdp_result = co_await client.describe();
    CO_ASSERT_TRUE(sdp_result.has_value());

    const auto& sdp = *sdp_result;
    spdlog::info("세션 이름: {}", sdp.session_name());
    spdlog::info("미디어 트랙 수: {}", sdp.media_descriptions().size());

    // 최소 하나의 비디오 트랙 존재
    EXPECT_TRUE(sdp.has_video());

    for (const auto& media : sdp.media_descriptions()) {
      auto type_str = (media.type == nx::sdp::SdpMediaType::kVideo) ? "video" : "audio";
      spdlog::info(
        "  미디어 타입: {}, 프로토콜: {}, Control: {}",
        type_str,
        media.protocol,
        media.control_url);

      for (const auto& [pt, codec] : media.rtpmap) {
        spdlog::info("    rtpmap: {} -> {}", pt, codec);
      }
    }

    co_await client.close();
  };

  m_runner->run_sync(test_logic(), nx::seconds(15));
}

// SETUP (TCP Interleaved) 테스트
TEST_F(RtspClientIntegrationTest, SetupTcpInterleaved)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);

    auto sdp_result = co_await client.describe();
    CO_ASSERT_TRUE(sdp_result.has_value());

    ec = co_await client.setup(nx::net::RtspTransport::kRtpTcp);
    CO_ASSERT_FALSE(ec);

    EXPECT_EQ(client.state(), nx::net::RtspSessionState::kReady);

    co_await client.teardown();
    co_await client.close();
  };

  m_runner->run_sync(test_logic(), nx::seconds(15));
}

// 전체 흐름 테스트 (Connect -> Describe -> Setup -> Play -> Teardown)
TEST_F(RtspClientIntegrationTest, FullStreamingFlow)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    // 프레임 수신 카운터
    std::atomic<uint32_t> frame_count{0};
    std::atomic<uint32_t> keyframe_count{0};

    client.set_media_callback([&frame_count, &keyframe_count](
                                uint32_t track_id,
                                nx::media::MediaType media_type,
                                std::shared_ptr<std::vector<uint8_t>> frame_data,
                                uint64_t timestamp_us,
                                bool keyframe) {
      frame_count++;
      if (keyframe) {
        keyframe_count++;
      }

      if (frame_count <= 5 || keyframe) {
        auto type_str = (media_type == nx::media::MediaType::kVideo) ? "video" : "audio";
        spdlog::info(
          "프레임 수신: track={}, type={}, size={}, ts={}, key={}",
          track_id,
          type_str,
          frame_data->size(),
          timestamp_us,
          keyframe);
      }
    });

    // 1. 연결
    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);

    // 2. DESCRIBE
    auto sdp_result = co_await client.describe();
    CO_ASSERT_TRUE(sdp_result.has_value());

    // 3. SETUP (TCP)
    ec = co_await client.setup(nx::net::RtspTransport::kRtpTcp);
    CO_ASSERT_FALSE(ec);

    // 4. PLAY
    ec = co_await client.play();
    CO_ASSERT_FALSE(ec);

    EXPECT_EQ(client.state(), nx::net::RtspSessionState::kPlaying);

    // 5. 5초간 스트림 수신
    co_await AsioSteadyTimer(m_runner->io_context(), nx::seconds(5))
      .async_wait(boost::asio::use_awaitable);

    spdlog::info(
      "수신한 프레임 수: {}, 키프레임: {}",
      frame_count.load(),
      keyframe_count.load());

    // 프레임이 수신되었는지 확인
    EXPECT_GT(frame_count.load(), 0u);

    // 6. TEARDOWN
    co_await client.teardown();
    co_await client.close();
  };

  m_runner->run_sync(test_logic(), nx::seconds(30));
}

// 여러 스트림 URL 테스트
TEST_F(RtspClientIntegrationTest, MultipleStreamUrls)
{
  const char* urls[] = {kRtspUrl1, kRtspUrl2, kRtspUrl3};

  for (const auto* url : urls) {
    auto test_logic = [this, url]() -> nx::awaitable<void> {
      nx::net::RtspClient client(m_runner->io_context());
      client.set_credentials(kUsername, kPassword);

      auto ec = co_await client.connect(url);
      CO_ASSERT_FALSE(ec);

      auto sdp_result = co_await client.describe();
      CO_ASSERT_TRUE(sdp_result.has_value());

      spdlog::info(
        "URL: {} -> 미디어 트랙 수: {}",
        url,
        sdp_result->media_descriptions().size());

      co_await client.close();
    };

    m_runner->run_sync(test_logic(), nx::seconds(15));
  }
}

// RTP 패킷 파싱 및 통계 검증 테스트
TEST_F(RtspClientIntegrationTest, RtpPacketParsingAndStatistics)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    // 프레임별 타임스탬프 순서 검증용
    std::atomic<uint64_t> last_video_ts{0};
    std::atomic<uint32_t> timestamp_order_violations{0};
    std::atomic<uint32_t> frame_count{0};
    std::atomic<uint64_t> total_frame_bytes{0};
    std::atomic<uint32_t> keyframe_count{0};

    client.set_media_callback([&](
                                uint32_t,
                                nx::media::MediaType media_type,
                                std::shared_ptr<std::vector<uint8_t>> frame_data,
                                uint64_t timestamp_us,
                                bool keyframe) {
      if (media_type == nx::media::MediaType::kVideo) {
        frame_count++;
        total_frame_bytes += frame_data->size();
        if (keyframe) {
          keyframe_count++;
        }

        // 타임스탬프 비감소(non-decreasing) 검증
        auto prev = last_video_ts.exchange(timestamp_us);
        if (prev > 0 && timestamp_us < prev) {
          timestamp_order_violations++;
        }

        // 프레임 데이터가 비어있지 않은지 검증
        EXPECT_GT(frame_data->size(), 0u);
      }
    });

    // 연결 -> DESCRIBE -> SETUP -> PLAY
    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);

    auto sdp_result = co_await client.describe();
    CO_ASSERT_TRUE(sdp_result.has_value());

    ec = co_await client.setup(nx::net::RtspTransport::kRtpTcp);
    CO_ASSERT_FALSE(ec);

    // SETUP 후 트랙 수 검증
    EXPECT_GT(client.track_count(), 0u);

    // 트랙 0은 비디오여야 함
    auto track0_type = client.get_track_media_type(0);
    CO_ASSERT_TRUE(track0_type.has_value());
    EXPECT_EQ(*track0_type, nx::media::MediaType::kVideo);

    ec = co_await client.play();
    CO_ASSERT_FALSE(ec);

    // 3초간 스트림 수신
    co_await AsioSteadyTimer(m_runner->io_context(), nx::seconds(3))
      .async_wait(boost::asio::use_awaitable);

    // ================================================================
    // RTP 통계 검증
    // ================================================================
    for (uint32_t i = 0; i < client.track_count(); ++i) {
      auto stats = client.get_track_statistics(i);
      CO_ASSERT_TRUE(stats.has_value());

      auto mtype = client.get_track_media_type(i);
      auto type_str = (!mtype.has_value())                       ? "unknown"
                      : (*mtype == nx::media::MediaType::kVideo) ? "video"
                      : (*mtype == nx::media::MediaType::kAudio) ? "audio"
                                                                 : "metadata";

      spdlog::info("트랙 {} ({}) RTP 통계:", i, type_str);
      spdlog::info("  수신 패킷: {}", stats->packets_received);
      spdlog::info("  손실 패킷: {}", stats->packets_lost);
      spdlog::info("  순서 이탈: {}", stats->packets_out_of_order);
      spdlog::info("  수신 바이트: {}", stats->bytes_received);

      // 패킷이 수신되었는지
      EXPECT_GT(stats->packets_received, 0u)
        << "트랙 " << i << " (" << type_str << ") RTP 패킷 미수신";

      // 바이트가 수신되었는지
      EXPECT_GT(stats->bytes_received, 0u)
        << "트랙 " << i << " (" << type_str << ") 바이트 미수신";
    }

    // 비디오 트랙 세부 검증
    auto video_stats = client.get_track_statistics(0);
    CO_ASSERT_TRUE(video_stats.has_value());

    // 비디오 패킷은 충분히 수신되어야 함 (3초 * 30fps ≈ 90+ 패킷)
    EXPECT_GT(video_stats->packets_received, 10u) << "비디오 RTP 패킷이 너무 적음";

    // 패킷 손실률 확인 (10% 이하)
    if (video_stats->packets_received > 0) {
      double loss_rate = static_cast<double>(video_stats->packets_lost)
                         / static_cast<double>(
                           video_stats->packets_received + video_stats->packets_lost);
      spdlog::info("비디오 패킷 손실률: {:.2f}%", loss_rate * 100.0);
      EXPECT_LT(loss_rate, 0.10) << "비디오 패킷 손실률이 10% 초과";
    }

    // H.264 프레임 검증
    spdlog::info(
      "H.264 프레임 수: {}, 키프레임: {}, 총 바이트: {}",
      frame_count.load(),
      keyframe_count.load(),
      total_frame_bytes.load());

    EXPECT_GT(frame_count.load(), 0u) << "H.264 프레임 미수신";
    EXPECT_GT(keyframe_count.load(), 0u) << "H.264 키프레임 미수신";
    EXPECT_GT(total_frame_bytes.load(), 0u) << "프레임 데이터 없음";

    // 타임스탬프 순서 위반 검증
    spdlog::info("타임스탬프 순서 위반: {}", timestamp_order_violations.load());
    EXPECT_EQ(timestamp_order_violations.load(), 0u)
      << "비디오 타임스탬프가 역순으로 전달됨";

    // 디패킷타이징 검증: 수신 패킷 수 > 프레임 수 (여러 패킷이 하나의 프레임으로
    // 조립)
    spdlog::info(
      "패킷:프레임 비율 = {}:{}",
      video_stats->packets_received,
      frame_count.load());
    EXPECT_GE(video_stats->packets_received, frame_count.load())
      << "디패킷타이징 비율 비정상 (패킷 < 프레임)";

    co_await client.teardown();
    co_await client.close();
  };

  m_runner->run_sync(test_logic(), nx::seconds(30));
}

// RTP 시퀀스 번호 연속성 및 수신 안정성 검증 (장시간)
TEST_F(RtspClientIntegrationTest, RtpSequenceStability)
{
  auto test_logic = [this]() -> nx::awaitable<void> {
    nx::net::RtspClient client(m_runner->io_context());
    client.set_credentials(kUsername, kPassword);

    auto ec = co_await client.connect(kRtspUrl1);
    CO_ASSERT_FALSE(ec);

    auto sdp_result = co_await client.describe();
    CO_ASSERT_TRUE(sdp_result.has_value());

    ec = co_await client.setup(nx::net::RtspTransport::kRtpTcp);
    CO_ASSERT_FALSE(ec);

    ec = co_await client.play();
    CO_ASSERT_FALSE(ec);

    // 5초간 수신
    co_await AsioSteadyTimer(m_runner->io_context(), nx::seconds(5))
      .async_wait(boost::asio::use_awaitable);

    auto stats = client.get_track_statistics(0);
    CO_ASSERT_TRUE(stats.has_value());

    spdlog::info("RTP 시퀀스 안정성 검증 (5초):");
    spdlog::info(
      "  수신: {} 패킷, 손실: {}, 순서 이탈: {}",
      stats->packets_received,
      stats->packets_lost,
      stats->packets_out_of_order);

    // TCP Interleaved에서는 패킷 손실이 0이어야 함
    EXPECT_EQ(stats->packets_lost, 0u) << "TCP Interleaved 모드에서 패킷 손실 발생";

    // TCP에서 순서 이탈도 0이어야 함
    EXPECT_EQ(stats->packets_out_of_order, 0u)
      << "TCP Interleaved 모드에서 패킷 순서 이탈 발생";

    co_await client.teardown();
    co_await client.close();
  };

  m_runner->run_sync(test_logic(), nx::seconds(30));
}

// ============================================================================
// 공개 RTSP 스트림 테스트 (인증 없이 접속)
// ============================================================================

class PublicRtspStreamTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_runner = std::make_unique<test::IoContextTestRunner>();
    m_runner->start(2);
  }

  void TearDown() override { m_runner->stop(); }

  std::unique_ptr<test::IoContextTestRunner> m_runner;
};

// 공개 스트림 연결 및 DESCRIBE 테스트
TEST_F(PublicRtspStreamTest, ConnectAndDescribe)
{
  const std::vector<std::string> urls = {
    "rtsp://210.99.70.120:1935/live/cctv001.stream",
    "rtsp://210.99.70.120:1935/live/cctv045.stream",
  };

  for (const auto& url : urls) {
    spdlog::info("========== 공개 스트림 테스트: {} ==========", url);

    auto test_logic = [this, &url]() -> nx::awaitable<void> {
      nx::net::RtspClient client(m_runner->io_context());
      // 인증 없이 접속

      auto ec = co_await client.connect(url);
      CO_ASSERT_FALSE(ec);
      EXPECT_TRUE(client.is_connected());

      auto sdp_result = co_await client.describe();
      CO_ASSERT_TRUE(sdp_result.has_value());

      const auto& sdp = *sdp_result;
      spdlog::info("세션 이름: {}", sdp.session_name());
      spdlog::info("미디어 트랙 수: {}", sdp.media_descriptions().size());
      EXPECT_GT(sdp.media_descriptions().size(), 0u) << "미디어 트랙이 없음: " << url;

      for (const auto& media : sdp.media_descriptions()) {
        auto type_str = (media.type == nx::sdp::SdpMediaType::kVideo)   ? "video"
                        : (media.type == nx::sdp::SdpMediaType::kAudio) ? "audio"
                                                                        : "application";
        spdlog::info(
          "  미디어: {}, 프로토콜: {}, Control: {}",
          type_str,
          media.protocol,
          media.control_url);
        for (const auto& [pt, codec] : media.rtpmap) {
          spdlog::info("    rtpmap: {} -> {}", pt, codec);
        }
      }

      co_await client.close();
    };

    m_runner->run_sync(test_logic(), nx::seconds(15));
  }
}

// 공개 스트림 전체 스트리밍 흐름 테스트
TEST_F(PublicRtspStreamTest, FullStreamingFlow)
{
  const std::vector<std::string> urls = {
    "rtsp://210.99.70.120:1935/live/cctv001.stream",
    "rtsp://210.99.70.120:1935/live/cctv045.stream",
  };

  for (const auto& url : urls) {
    spdlog::info("========== 공개 스트림 스트리밍: {} ==========", url);

    auto test_logic = [this, &url]() -> nx::awaitable<void> {
      nx::net::RtspClient client(m_runner->io_context());

      std::atomic<uint32_t> frame_count{0};
      std::atomic<uint64_t> total_bytes{0};
      std::atomic<uint32_t> keyframe_count{0};

      client.set_media_callback([&](
                                  uint32_t /*track_id*/,
                                  nx::media::MediaType media_type,
                                  std::shared_ptr<std::vector<uint8_t>> frame_data,
                                  uint64_t /*timestamp_us*/,
                                  bool keyframe) {
        if (media_type == nx::media::MediaType::kVideo) {
          frame_count++;
          total_bytes += frame_data->size();
          if (keyframe) {
            keyframe_count++;
          }
        }
      });

      auto ec = co_await client.connect(url);
      CO_ASSERT_FALSE(ec);

      auto sdp_result = co_await client.describe();
      CO_ASSERT_TRUE(sdp_result.has_value());

      ec = co_await client.setup(nx::net::RtspTransport::kRtpTcp);
      CO_ASSERT_FALSE(ec);

      ec = co_await client.play();
      CO_ASSERT_FALSE(ec);
      EXPECT_EQ(client.state(), nx::net::RtspSessionState::kPlaying);

      // 5초간 스트림 수신
      co_await AsioSteadyTimer(m_runner->io_context(), nx::seconds(5))
        .async_wait(boost::asio::use_awaitable);

      spdlog::info(
        "결과 - 프레임: {}, 키프레임: {}, 데이터: {} KB",
        frame_count.load(),
        keyframe_count.load(),
        total_bytes.load() / 1024);

      EXPECT_GT(frame_count.load(), 0u) << "비디오 프레임 미수신: " << url;

      // 트랙별 RTP 통계 출력
      for (uint32_t i = 0; i < client.track_count(); ++i) {
        auto stats = client.get_track_statistics(i);
        if (stats.has_value()) {
          auto mtype = client.get_track_media_type(i);
          auto type_str = (!mtype.has_value())                       ? "unknown"
                          : (*mtype == nx::media::MediaType::kVideo) ? "video"
                          : (*mtype == nx::media::MediaType::kAudio) ? "audio"
                                                                     : "other";
          spdlog::info(
            "  트랙 {} ({}): 패킷={}, 손실={}, 순서이탈={}, 바이트={}",
            i,
            type_str,
            stats->packets_received,
            stats->packets_lost,
            stats->packets_out_of_order,
            stats->bytes_received);
        }
      }

      co_await client.teardown();
      co_await client.close();
    };

    m_runner->run_sync(test_logic(), nx::seconds(30));
  }
}
