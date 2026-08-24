// 파일: onvif_client_integration_test.cpp
// 생성일: 2026-02-20
// 설명: OnvifClient 통합 테스트 (실제 카메라 연동)

#include <nxnet/onvif/onvif_client.h>
#include <nxnet/onvif/onvif_types.h>
#include <nxnet/onvif/onvif_error.h>
#include <nxnet/onvif/services/media_service.h>
#include <nxcore/media/media_codec.h>
#include <tests/common/io_context_test_runner.h>
#include <tests/common/coroutine_helper.h>

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <string>
#include <vector>

namespace {

// ============================================================================
// 카메라 연결 정보
// ============================================================================
constexpr const char* kCameraHost = "192.168.0.168";
constexpr int kCameraPort = 80;
constexpr const char* kCameraUser = "admin";
constexpr const char* kCameraPassword = "cctv3200*";

// ============================================================================
// 출력 헬퍼 함수
// ============================================================================

void
print_separator(const std::string& title)
{
  std::cout << "\n" << std::string(60, '=') << "\n";
  std::cout << "  " << title << "\n";
  std::cout << std::string(60, '=') << "\n";
}

void
print_field(const std::string& key, const std::string& value, int indent = 2)
{
  std::cout << std::string(indent, ' ') << std::left << std::setw(24) << key << ": "
            << value << "\n";
}

} // namespace

// ============================================================================
// 통합 테스트 픽스처
// ============================================================================

class OnvifClientIntegrationTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_runner.start(4);

    m_client = std::make_unique<nx::net::onvif::OnvifClient>(
      m_runner.io_context(),
      kCameraHost,
      kCameraPort,
      kCameraUser,
      kCameraPassword);
  }

  void TearDown() override
  {
    m_client.reset();
    m_runner.stop();
  }

  /// 초기화 공통 헬퍼 (코루틴 내부에서 호출)
  nx::awaitable<bool> ensure_initialized()
  {
    auto result = co_await m_client->initialize();
    if (!result.has_value()) {
      ADD_FAILURE() << "초기화 실패: " << result.error().message();
      co_return false;
    }
    co_return true;
  }

  test::IoContextTestRunner m_runner;
  std::unique_ptr<nx::net::onvif::OnvifClient> m_client;
};

// ============================================================================
// 초기화 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, Initialize_Success)
{
  EXPECT_FALSE(m_client->is_initialized());

  auto test_logic = [&]() -> nx::awaitable<void> {
    auto result = co_await m_client->initialize();
    CO_ASSERT_TRUE(result.has_value());

    EXPECT_TRUE(m_client->is_initialized());

    print_separator("ONVIF Client 초기화 성공");
    print_field("Host", m_client->get_host());
    print_field("Port", std::to_string(m_client->get_port()));
    print_field("Initialized", "true");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// DateTime 캐시 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, Initialize_GetDateTime_Cached)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto dt = m_client->get_system_date_time();
    CO_ASSERT_TRUE(dt.has_value());

    EXPECT_GE(dt->year, 2020);
    EXPECT_GE(dt->month, 1);
    EXPECT_LE(dt->month, 12);
    EXPECT_GE(dt->day, 1);
    EXPECT_LE(dt->day, 31);

    print_separator("카메라 시간 정보");
    print_field("ISO 8601", dt->to_iso8601());
    print_field("Year", std::to_string(dt->year));
    print_field("Month", std::to_string(dt->month));
    print_field("Day", std::to_string(dt->day));
    print_field("Hour", std::to_string(dt->hour));
    print_field("Minute", std::to_string(dt->minute));
    print_field("Second", std::to_string(dt->second));
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// Capabilities 캐시 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, Initialize_GetCapabilities_Cached)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto caps = m_client->get_capabilities();
    CO_ASSERT_TRUE(caps.has_value());

    EXPECT_FALSE(caps->media.media_service_url.empty());

    print_separator("카메라 Capabilities");
    print_field("Device Service URL", caps->device.device_service_url);
    print_field("Media Service URL", caps->media.media_service_url);
    print_field("Streaming", caps->media.streaming_supported ? "Yes" : "No");
    print_field("Snapshot", caps->media.snapshot_supported ? "Yes" : "No");

    if (caps->ptz.has_value()) {
      print_field("PTZ Service URL", caps->ptz->ptz_service_url);
    }
    else {
      print_field("PTZ", "Not Supported");
    }

    if (caps->events.has_value()) {
      print_field("Event Service URL", caps->events->event_service_url);
      print_field("PullPoint", caps->events->pull_point_supported ? "Yes" : "No");
    }

    if (caps->imaging.has_value()) {
      print_field("Imaging Service URL", caps->imaging->imaging_service_url);
    }
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// DeviceInformation 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, GetDeviceInformation_ValidData)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto info_result = co_await m_client->get_device_information();
    CO_ASSERT_TRUE(info_result.has_value());

    auto& info = info_result.value();
    EXPECT_FALSE(info.manufacturer.empty());
    EXPECT_FALSE(info.model.empty());

    print_separator("장치 정보 (Device Information)");
    print_field("Manufacturer", info.manufacturer);
    print_field("Model", info.model);
    print_field("Firmware Version", info.firmware_version);
    print_field("Serial Number", info.serial_number);
    print_field("Hardware ID", info.hardware_id);
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// Profiles 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, GetProfiles_NotEmpty)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto profiles_result = co_await m_client->get_profiles();
    CO_ASSERT_TRUE(profiles_result.has_value());

    auto& profiles = profiles_result.value();
    EXPECT_GE(profiles.size(), 1u);

    print_separator("미디어 프로파일 목록 (" + std::to_string(profiles.size()) + "개)");

    for (size_t i = 0; i < profiles.size(); ++i) {
      const auto& p = profiles[i];
      std::cout << "\n  [Profile " << (i + 1) << "]\n";
      print_field("Token", p.token, 4);
      print_field("Name", p.name, 4);
      print_field("Fixed", p.fixed ? "Yes" : "No", 4);

      if (p.video_encoder.has_value()) {
        const auto& ve = *p.video_encoder;
        print_field(
          "Video Codec",
          std::string(nx::media::video_codec_to_string(ve.codec)),
          4);
        print_field(
          "Resolution",
          std::to_string(ve.resolution.width) + "x"
            + std::to_string(ve.resolution.height),
          4);
        print_field("Framerate", std::to_string(ve.framerate) + " fps", 4);
        print_field("Bitrate", std::to_string(ve.bitrate / 1000) + " Kbps", 4);
        print_field("Quality", std::to_string(ve.quality), 4);
        print_field("GOP Size", std::to_string(ve.gop_size), 4);
        if (!ve.profile.empty()) {
          print_field("H264 Profile", ve.profile, 4);
        }
      }

      if (p.audio_encoder.has_value()) {
        const auto& ae = *p.audio_encoder;
        print_field(
          "Audio Codec",
          std::string(nx::media::audio_codec_to_string(ae.codec)),
          4);
        print_field("Audio Bitrate", std::to_string(ae.bitrate / 1000) + " Kbps", 4);
        print_field("Sample Rate", std::to_string(ae.sample_rate) + " Hz", 4);
      }

      if (p.ptz_config.has_value()) {
        print_field("PTZ Config", p.ptz_config->name, 4);
        print_field("PTZ Node Token", p.ptz_config->node_token, 4);
      }
    }
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(OnvifClientIntegrationTest, GetProfiles_ValidFields)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto profiles_result = co_await m_client->get_profiles();
    CO_ASSERT_TRUE(profiles_result.has_value());

    for (const auto& p : profiles_result.value()) {
      EXPECT_FALSE(p.token.empty()) << "프로파일 토큰이 비어있음";
      EXPECT_FALSE(p.name.empty()) << "프로파일 이름이 비어있음: token=" << p.token;

      if (p.video_encoder.has_value()) {
        EXPECT_NE(p.video_encoder->codec, nx::net::onvif::VideoCodec::kUnknown)
          << "비디오 코덱이 Unknown: token=" << p.token;
        EXPECT_GT(p.video_encoder->resolution.width, 0)
          << "비디오 width가 0: token=" << p.token;
        EXPECT_GT(p.video_encoder->resolution.height, 0)
          << "비디오 height가 0: token=" << p.token;
      }
    }
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// Stream URI 테스트
// ============================================================================

TEST_F(OnvifClientIntegrationTest, GetMainStreamUri_ValidRtspUri)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto uri_result = co_await m_client->get_main_stream_uri();

    if (!uri_result.has_value()) {
      print_separator("Main Stream URI - ERROR");
      print_field("Error Code", std::to_string(uri_result.error().value()));
      print_field("Error Message", uri_result.error().message());
      print_field("Error Category", uri_result.error().category().name());
    }

    CO_ASSERT_TRUE(uri_result.has_value());

    auto& stream = uri_result.value();
    EXPECT_FALSE(stream.uri.empty());
    EXPECT_EQ(stream.uri.substr(0, 7), "rtsp://");

    print_separator("Main Stream URI");
    print_field("URI", stream.uri);
    print_field("Invalid After Connect", stream.invalid_after_connect ? "Yes" : "No");
    print_field("Invalid After Reboot", stream.invalid_after_reboot ? "Yes" : "No");
    print_field("Timeout", std::to_string(stream.timeout.count()) + "s");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(OnvifClientIntegrationTest, GetSecondStreamUri_ValidRtspUri)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    auto uri_result = co_await m_client->get_second_stream_uri();
    CO_ASSERT_TRUE(uri_result.has_value());

    auto& stream = uri_result.value();
    EXPECT_FALSE(stream.uri.empty());
    EXPECT_EQ(stream.uri.substr(0, 7), "rtsp://");

    print_separator("Second Stream URI");
    print_field("URI", stream.uri);
    print_field("Invalid After Connect", stream.invalid_after_connect ? "Yes" : "No");
    print_field("Invalid After Reboot", stream.invalid_after_reboot ? "Yes" : "No");
    print_field("Timeout", std::to_string(stream.timeout.count()) + "s");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(OnvifClientIntegrationTest, GetMainStreamUri_ReplaceHost)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    // 호스트를 10.0.0.1로 교체
    auto uri_result = co_await m_client->get_main_stream_uri("10.0.0.1");
    CO_ASSERT_TRUE(uri_result.has_value());

    auto& stream = uri_result.value();

    // 교체된 호스트가 URI에 포함되어야 함
    EXPECT_NE(stream.uri.find("10.0.0.1"), std::string::npos)
      << "호스트가 교체되지 않음: " << stream.uri;

    print_separator("Main Stream URI (호스트 교체)");
    print_field("URI", stream.uri);
    print_field("Replace Host", "10.0.0.1");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(OnvifClientIntegrationTest, GetStreamUri_UnknownToken_Error)
{
  auto test_logic = [&]() -> nx::awaitable<void> {
    if (!co_await ensure_initialized()) {
      co_return;
    }

    // 존재하지 않는 프로파일 토큰으로 요청
    auto uri_result = co_await m_client->get_stream_uri("NONEXISTENT_TOKEN_12345");

    // 에러가 반환되어야 함
    EXPECT_FALSE(uri_result.has_value()) << "존재하지 않는 토큰에 대해 성공이 반환됨";

    if (!uri_result.has_value()) {
      print_separator("잘못된 토큰 에러 확인");
      print_field("Error Code", std::to_string(uri_result.error().value()));
      print_field("Error Message", uri_result.error().message());
    }
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}
