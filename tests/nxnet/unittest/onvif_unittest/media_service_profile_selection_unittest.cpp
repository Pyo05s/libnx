// 파일: media_service_profile_selection_unittest.cpp
// 생성일: 2026-02-19
// 설명: MediaService 프로파일 선택 알고리즘 단위 테스트 (네트워크 불필요)

#include <nxnet/onvif/services/media_service.h>
#include <nxnet/onvif/onvif_types.h>

#include <gtest/gtest.h>
#include <vector>
#include <string>

namespace nx::net::onvif::services {

// ============================================================================
// 테스트 헬퍼
// ============================================================================

namespace {

/// 테스트용 MediaProfile 생성 헬퍼
MediaProfile
make_profile(
  const std::string& token,
  VideoCodec codec,
  int width,
  int height,
  int bitrate = 2048000)
{
  // VideoEncoderConfig 구성
  VideoEncoderConfig encoder{
    .token = token + "_enc",
    .name = "Encoder_" + token,
    .codec = codec,
    .resolution = VideoResolution{.width = width, .height = height},
    .bitrate = bitrate
  };

  MediaProfile profile{
    .token = token,
    .name = "Profile_" + token,
    .video_encoder = std::move(encoder)};

  return profile;
}

} // namespace

// ============================================================================
// select_main_profile - 코덱 우선순위 테스트
// ============================================================================

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_H264_Over_H265)
{
  // 동일 해상도에서 H.264가 H.265보다 높은 우선순위 획득 검증
  std::vector<MediaProfile> profiles = {
    make_profile("h265_720p", VideoCodec::kH265, 1280, 720),
    make_profile("h264_720p", VideoCodec::kH264, 1280, 720)};

  auto selected = MediaService::select_main_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "h264_720p");
}

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_H264_Over_MJPEG)
{
  // 동일 해상도에서 H.264가 MJPEG보다 높은 우선순위 획득 검증
  std::vector<MediaProfile> profiles = {
    make_profile("mjpeg_720p", VideoCodec::kMjpeg, 1280, 720),
    make_profile("h264_720p", VideoCodec::kH264, 1280, 720)};

  auto selected = MediaService::select_main_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "h264_720p");
}

// ============================================================================
// select_main_profile - 해상도 우선순위 테스트
// ============================================================================

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_PreferHighResolution)
{
  // 동일 코덱에서 고해상도 프로파일 우선 선택 검증
  std::vector<MediaProfile> profiles = {
    make_profile("h264_480p", VideoCodec::kH264, 640, 480),
    make_profile("h264_720p", VideoCodec::kH264, 1280, 720),
    make_profile("h264_1080p", VideoCodec::kH264, 1920, 1080)};

  auto selected = MediaService::select_main_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "h264_1080p");
}

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_1080pBonus)
{
  // 1080p 해상도 보너스로 인해 1080p H.265가 720p H.264보다 선택됨 검증
  std::vector<MediaProfile> profiles = {
    make_profile("h264_720p", VideoCodec::kH264, 1280, 720),
    make_profile("h265_1080p", VideoCodec::kH265, 1920, 1080)};

  // Main 스트림 선호 해상도: 1920x1080 → 1080p H.265가 보너스로 우선 선택
  auto selected = MediaService::select_main_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "h265_1080p");
}

// ============================================================================
// select_second_profile - 낮은 해상도 선호 테스트
// ============================================================================

TEST(MediaServiceProfileSelectionTest, SelectSecondProfile_PreferLowResolution)
{
  // 서브스트림 선택 시 낮은 해상도 프로파일 우선 선택 검증
  std::vector<MediaProfile> profiles = {
    make_profile("h264_1080p", VideoCodec::kH264, 1920, 1080),
    make_profile("h264_720p", VideoCodec::kH264, 1280, 720),
    make_profile("h264_480p", VideoCodec::kH264, 640, 480)};

  // Second 스트림 선호 해상도: 1280x720 → 720p 또는 그 이하 선택
  auto selected = MediaService::select_second_profile(profiles);

  ASSERT_TRUE(selected.has_value());

  // 1920x1080가 선택되지 않아야 함 (고해상도 기피)
  EXPECT_NE(selected->token, "h264_1080p");
}

TEST(MediaServiceProfileSelectionTest, SelectSecondProfile_720pBonus)
{
  // Second 스트림 선택 알고리즘 검증: 최저점 프로파일 선택
  // 720p 정확 일치 프로파일은 +5000 보너스로 최고점이 됨 → second stream이 회피
  // 따라서 second stream에서는 720p보다 낮은 해상도 프로파일이 선택됨
  std::vector<MediaProfile> profiles = {
    make_profile("h264_1080p", VideoCodec::kH264, 1920, 1080),
    make_profile("h264_480p", VideoCodec::kH264, 640, 480)};

  // Second 스트림: 코덱+해상도 점수가 낙은 h264_480p 선택
  auto selected = MediaService::select_second_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "h264_480p");
}

// ============================================================================
// 경계 조건 테스트
// ============================================================================

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_EmptyList)
{
  // 빈 목록 입력 시 nullopt 반환 검증
  std::vector<MediaProfile> empty_profiles;

  auto selected = MediaService::select_main_profile(empty_profiles);

  EXPECT_FALSE(selected.has_value());
}

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_SingleProfile)
{
  // 프로파일이 1개일 때 해당 프로파일 반환 검증
  std::vector<MediaProfile> profiles
    = {make_profile("only_profile", VideoCodec::kH264, 1280, 720)};

  auto selected = MediaService::select_main_profile(profiles);

  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "only_profile");
}

TEST(MediaServiceProfileSelectionTest, SelectSecondProfile_EmptyList)
{
  // 빈 목록에서 서브프로파일 선택 시 nullopt 반환 검증
  std::vector<MediaProfile> empty_profiles;

  auto selected = MediaService::select_second_profile(empty_profiles);

  EXPECT_FALSE(selected.has_value());
}

TEST(MediaServiceProfileSelectionTest, SelectMainProfile_NoVideoEncoder)
{
  // video_encoder가 없는 프로파일은 선택에서 제외 검증
  std::vector<MediaProfile> profiles;

  // video_encoder 없는 프로파일 추가
  MediaProfile no_enc_profile;
  no_enc_profile.token = "no_encoder";
  no_enc_profile.name = "NoEncoder";
  no_enc_profile.video_encoder = std::nullopt;
  profiles.push_back(std::move(no_enc_profile));

  // video_encoder 있는 정상 프로파일 추가
  profiles.push_back(make_profile("valid_profile", VideoCodec::kH264, 1280, 720));

  auto selected = MediaService::select_main_profile(profiles);

  // 유효한 프로파일이 선택됨
  ASSERT_TRUE(selected.has_value());
  EXPECT_EQ(selected->token, "valid_profile");
}

} // namespace nx::net::onvif::services
