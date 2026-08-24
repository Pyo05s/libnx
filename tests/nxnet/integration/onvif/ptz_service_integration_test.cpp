// 파일: ptz_service_integration_test.cpp
// 생성일: 2026-02-20
// 설명: PTZ Service 통합 테스트 (실제 카메라 연동)

#include <nxnet/onvif/onvif_client.h>
#include <nxnet/onvif/onvif_types.h>
#include <nxnet/onvif/services/ptz_service.h>
#include <tests/common/io_context_test_runner.h>
#include <tests/common/coroutine_helper.h>

#include <gtest/gtest.h>
#include <iostream>
#include <iomanip>
#include <string>

namespace {

// ============================================================================
// 카메라 연결 정보
// ============================================================================
constexpr const char* kCameraHost = "192.168.0.168";
constexpr int kCameraPort = 80;
constexpr const char* kCameraUser = "admin";
constexpr const char* kCameraPassword = "cctv3200*";

// ============================================================================
// 출력 헬퍼
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

class PtzServiceIntegrationTest : public ::testing::Test
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

    // 초기화 및 첫 번째 프로파일 토큰 획득
    auto init_logic = [&]() -> nx::awaitable<void> {
      auto init_result = co_await m_client->initialize();
      if (!init_result.has_value()) {
        co_return;
      }

      auto profiles_result = co_await m_client->get_profiles();
      if (profiles_result.has_value() && !profiles_result->empty()) {
        m_profile_token = profiles_result->front().token;
      }
    };

    try {
      m_runner.run_sync(init_logic(), nx::seconds(15));
    }
    catch (const std::exception& e) {
      std::cerr << "SetUp 실패: " << e.what() << std::endl;
    }
  }

  void TearDown() override
  {
    m_client.reset();
    m_runner.stop();
  }

  test::IoContextTestRunner m_runner;
  std::unique_ptr<nx::net::onvif::OnvifClient> m_client;
  std::string m_profile_token;
};

// ============================================================================
// ContinuousMove / Stop 테스트
// ============================================================================

TEST_F(PtzServiceIntegrationTest, ContinuousMove_Right_NoError)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    // pan_tilt_x = 0.3 으로 우측 이동
    auto result = co_await m_client->continuous_move(m_profile_token, 0.3, 0.0, 0.0, 2);

    CO_ASSERT_TRUE(result.has_value());

    print_separator("PTZ ContinuousMove (Right)");
    print_field("Profile Token", m_profile_token);
    print_field("Pan Speed", "0.3");
    print_field("Result", "Success");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(PtzServiceIntegrationTest, Stop_All_NoError)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    auto result = co_await m_client->stop(m_profile_token, true, true);

    CO_ASSERT_TRUE(result.has_value());

    print_separator("PTZ Stop (All)");
    print_field("Profile Token", m_profile_token);
    print_field("Pan/Tilt Stop", "Yes");
    print_field("Zoom Stop", "Yes");
    print_field("Result", "Success");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ============================================================================
// Preset CRUD 테스트
// ============================================================================

TEST_F(PtzServiceIntegrationTest, SetPreset_ReturnsToken)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    auto result = co_await m_client->set_preset(m_profile_token, "IntegrationTestPreset");

    CO_ASSERT_TRUE(result.has_value());
    EXPECT_FALSE(result->empty()) << "반환된 Preset 토큰이 비어있음";

    print_separator("PTZ SetPreset");
    print_field("Profile Token", m_profile_token);
    print_field("Preset Name", "IntegrationTestPreset");
    print_field("Preset Token", result.value());

    // 정리: 테스트 Preset 삭제
    [[maybe_unused]]
    auto cleanup = co_await m_client->remove_preset(m_profile_token, result.value());
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(PtzServiceIntegrationTest, GetPresets_ContainsSavedPreset)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    // Preset 생성
    auto set_result
      = co_await m_client->set_preset(m_profile_token, "IntegrationTestPreset2");
    CO_ASSERT_TRUE(set_result.has_value());

    std::string saved_token = set_result.value();

    // Preset 목록 조회
    auto list_result = co_await m_client->get_presets(m_profile_token);
    CO_ASSERT_TRUE(list_result.has_value());

    // 저장한 Preset이 목록에 포함되어야 함
    bool found = false;
    for (const auto& preset : list_result.value()) {
      if (preset.token == saved_token) {
        found = true;
        break;
      }
    }
    EXPECT_TRUE(found) << "저장한 Preset이 목록에 없음: " << saved_token;

    print_separator("PTZ GetPresets (" + std::to_string(list_result->size()) + "개)");
    for (const auto& preset : list_result.value()) {
      std::cout << "    [" << preset.token << "] " << preset.name << "\n";
    }

    // 정리: 테스트 Preset 삭제
    [[maybe_unused]]
    auto cleanup = co_await m_client->remove_preset(m_profile_token, saved_token);
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(PtzServiceIntegrationTest, GotoPreset_NoError)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    // Preset 생성
    auto set_result = co_await m_client->set_preset(m_profile_token, "GotoTestPreset");
    CO_ASSERT_TRUE(set_result.has_value());

    std::string preset_token = set_result.value();

    // GotoPreset 이동
    auto goto_result = co_await m_client->goto_preset(m_profile_token, preset_token);
    CO_ASSERT_TRUE(goto_result.has_value());

    print_separator("PTZ GotoPreset");
    print_field("Profile Token", m_profile_token);
    print_field("Preset Token", preset_token);
    print_field("Result", "Success");

    // 정리: 테스트 Preset 삭제
    [[maybe_unused]]
    auto cleanup = co_await m_client->remove_preset(m_profile_token, preset_token);
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(PtzServiceIntegrationTest, RemovePreset_NoError)
{
  if (m_profile_token.empty()) {
    GTEST_SKIP() << "프로파일 토큰 획득 실패 (PTZ 미지원 가능성)";
  }

  auto test_logic = [&]() -> nx::awaitable<void> {
    // Preset 생성
    auto set_result = co_await m_client->set_preset(m_profile_token, "RemoveTestPreset");
    CO_ASSERT_TRUE(set_result.has_value());

    std::string preset_token = set_result.value();

    // Preset 삭제
    auto remove_result = co_await m_client->remove_preset(m_profile_token, preset_token);
    CO_ASSERT_TRUE(remove_result.has_value());

    // 삭제 후 목록 조회 → 해당 토큰이 없어야 함
    auto list_result = co_await m_client->get_presets(m_profile_token);
    CO_ASSERT_TRUE(list_result.has_value());

    bool found = false;
    for (const auto& preset : list_result.value()) {
      if (preset.token == preset_token) {
        found = true;
        break;
      }
    }
    EXPECT_FALSE(found) << "삭제한 Preset이 목록에 남아있음: " << preset_token;

    print_separator("PTZ RemovePreset");
    print_field("Preset Token", preset_token);
    print_field("Removed", "Yes");
    print_field("Verified", found ? "Failed" : "OK");
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}
