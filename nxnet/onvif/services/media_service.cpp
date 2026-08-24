// 파일: media_service.cpp
// 생성일: 2026-02-19
// 설명: ONVIF Media Service 구현

#include "media_service.h"
#include "nxnet/onvif/soap/soap_envelope.h"
#include "nxnet/onvif/onvif_error.h"
#include <nxcore/util/xml_util.h>
#include <nxcore/util/debug_util.h>

#include <algorithm>
#include <cmath>

namespace nx::net::onvif::services {

namespace {

// 코덱별 우선순위 점수 (높을수록 선호)
constexpr int kCodecScoreH264 = 10000;
constexpr int kCodecScoreH265 = 9000;
constexpr int kCodecScoreMJPEG = 5000;
constexpr int kCodecScoreMPEG4 = 5000;

// Main/Second 스트림 기본 선호 해상도
constexpr int kMainPreferredWidth = 1920;
constexpr int kMainPreferredHeight = 1080;
constexpr int kSecondPreferredWidth = 1280;
constexpr int kSecondPreferredHeight = 720;

/// 코덱 문자열을 VideoCodec enum으로 변환
VideoCodec
parse_video_codec(const std::string& codec_str)
{
  return nx::media::video_codec_from_string(codec_str);
}

/// 오디오 코덱 문자열을 AudioCodec enum으로 변환
/// ONVIF 장비는 "G711alaw", "G726-32" 등 다양한 형식을 반환하므로
/// G711/G726은 부분 문자열 매칭 유지
AudioCodec
parse_audio_codec(const std::string& codec_str)
{
  // 부분 문자열 매칭이 필요한 코덱 먼저 처리
  if (
    codec_str.find("G711") != std::string::npos
    || codec_str.find("g711") != std::string::npos) {
    return AudioCodec::kG711;
  }
  if (
    codec_str.find("G726") != std::string::npos
    || codec_str.find("g726") != std::string::npos) {
    return AudioCodec::kG726;
  }
  return nx::media::audio_codec_from_string(codec_str);
}

} // anonymous namespace

// ============================================================================
// 생성자/소멸자
// ============================================================================

MediaService::MediaService(
  std::shared_ptr<soap::SoapClient> soap_client, std::string service_url)
    : m_soap_client(std::move(soap_client))
    , m_service_url(std::move(service_url))
{
  NX_ASSERT(m_soap_client != nullptr);
}

MediaService::~MediaService() = default;

// ============================================================================
// ONVIF Media Service 메서드
// ============================================================================

nx::awaitable_expected<std::vector<MediaProfile>>
MediaService::get_profiles()
{
  std::string action = "http://www.onvif.org/ver10/media/wsdl/GetProfiles";
  std::string body = soap::create_soap_body(
    "GetProfiles",
    "", // 파라미터 없음
    soap::ns::kOnvifMedia);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_profiles_response(response->body);
}

nx::awaitable_expected<StreamUri>
MediaService::get_stream_uri(
  const std::string& profile_token, StreamProtocol protocol, TransportType transport)
{
  std::string action = "http://www.onvif.org/ver10/media/wsdl/GetStreamUri";

  // StreamSetup 구성
  std::string protocol_str = (protocol == StreamProtocol::kRtsp) ? "RTSP" : "HTTP";
  std::string transport_str
    = (transport == TransportType::kUnicast) ? "RTP-Unicast" : "RTP-Multicast";

  // StreamSetup 내부 요소(Stream, Transport, Protocol)는 tt: 네임스페이스에 속함
  std::string schema_ns = soap::ns::kOnvifSchema;
  std::string body_content = "<StreamSetup>"
                             "<Stream xmlns=\""
                             + schema_ns + "\">" + transport_str
                             + "</Stream>"
                               "<Transport xmlns=\""
                             + schema_ns
                             + "\">"
                               "<Protocol>"
                             + protocol_str
                             + "</Protocol>"
                               "</Transport>"
                               "</StreamSetup>"
                               "<ProfileToken>"
                             + profile_token + "</ProfileToken>";

  std::string body
    = soap::create_soap_body("GetStreamUri", body_content, soap::ns::kOnvifMedia);

  DateTime current_time = DateTime::now_utc();

  auto response
    = co_await m_soap_client->send_request(m_service_url, action, body, current_time);

  if (!response.has_value()) {
    co_return std::unexpected(response.error());
  }

  co_return parse_stream_uri_response(response->body);
}

// ========================================================================
// 프로파일 선택 헬퍼
// ========================================================================

std::optional<MediaProfile>
MediaService::select_best_profile(
  const std::vector<MediaProfile>& profiles, const ProfileSelectionOptions& options)
{
  if (profiles.empty()) {
    return std::nullopt;
  }

  // 선호 해상도 결정
  int preferred_width = options.preferred_width;
  int preferred_height = options.preferred_height;

  if (preferred_width == 0 || preferred_height == 0) {
    // 용도에 따라 자동 설정
    if (options.purpose == StreamPurpose::kMain) {
      preferred_width = kMainPreferredWidth;
      preferred_height = kMainPreferredHeight;
    }
    else {
      preferred_width = kSecondPreferredWidth;
      preferred_height = kSecondPreferredHeight;
    }
  }

  // 각 프로파일의 점수 계산
  std::vector<std::pair<int, const MediaProfile*>> scored_profiles;

  for (const auto& profile : profiles) {
    if (!profile.video_encoder.has_value()) {
      continue; // 비디오 인코더가 없으면 스킵
    }

    int score = calculate_profile_score(profile);

    // 해상도 선호도 보너스
    int resolution_bonus
      = calculate_resolution_bonus(profile, preferred_width, preferred_height);

    score += resolution_bonus;

    scored_profiles.push_back({score, &profile});
  }

  if (scored_profiles.empty()) {
    return std::nullopt;
  }

  // 점수 기준 정렬 (내림차순)
  std::sort(
    scored_profiles.begin(),
    scored_profiles.end(),
    [](const auto& a, const auto& b) { return a.first > b.first; });

  // Main 스트림: 최고 점수 선택
  if (options.purpose == StreamPurpose::kMain) {
    return *scored_profiles[0].second;
  }

  // Second 스트림: 최저 점수 선택 (단, 유효한 프로파일 중)
  return *scored_profiles.back().second;
}

std::optional<MediaProfile>
MediaService::select_main_profile(const std::vector<MediaProfile>& profiles)
{
  ProfileSelectionOptions options;
  options.purpose = StreamPurpose::kMain;
  return select_best_profile(profiles, options);
}

std::optional<MediaProfile>
MediaService::select_second_profile(const std::vector<MediaProfile>& profiles)
{
  ProfileSelectionOptions options;
  options.purpose = StreamPurpose::kSecond;
  return select_best_profile(profiles, options);
}

// ========================================================================
// 서비스 URL 관리
// ========================================================================

std::string
MediaService::get_service_url() const
{
  return m_service_url;
}

void
MediaService::set_service_url(const std::string& url)
{
  m_service_url = url;
}

// ============================================================================
// 내부 파싱 메서드
// ============================================================================

nx::expected<std::vector<MediaProfile>>
MediaService::parse_profiles_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  std::vector<MediaProfile> profiles;

  // GetProfilesResponse 찾기
  auto response_node = doc.select_node("//*[local-name()='GetProfilesResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // Profiles 노드들 찾기
  auto profile_nodes = doc.select_nodes("//*[local-name()='Profiles']");

  for (const auto& profile_node : profile_nodes) {
    MediaProfile profile;

    // 프로파일 기본 정보
    profile.token = nx::get_node_attribute(profile_node.node(), "token");
    profile.fixed = nx::get_node_attribute(profile_node.node(), "fixed") == "true";

    auto name = nx::get_child_text(profile_node.node(), "Name");
    if (name) {
      profile.name = *name;
    }

    // VideoEncoderConfiguration 파싱
    auto video_encoder_node
      = nx::find_child_ignore_ns(profile_node.node(), "VideoEncoderConfiguration");
    if (video_encoder_node) {
      profile.video_encoder = parse_video_encoder_config(*video_encoder_node);
    }

    // AudioEncoderConfiguration 파싱
    auto audio_encoder_node
      = nx::find_child_ignore_ns(profile_node.node(), "AudioEncoderConfiguration");
    if (audio_encoder_node) {
      profile.audio_encoder = parse_audio_encoder_config(*audio_encoder_node);
    }

    // PTZConfiguration 파싱
    auto ptz_node = nx::find_child_ignore_ns(profile_node.node(), "PTZConfiguration");
    if (ptz_node) {
      profile.ptz_config = parse_ptz_config(*ptz_node);
    }

    profiles.push_back(std::move(profile));
  }

  if (profiles.empty()) {
    return std::unexpected(make_error_code(OnvifError::kNoProfile));
  }

  return profiles;
}

nx::expected<StreamUri>
MediaService::parse_stream_uri_response(const std::string& response_body)
{
  nx::XmlDocument doc;
  auto ec = doc.parse(response_body);
  if (ec) {
    return std::unexpected(make_error_code(OnvifError::kSoapParseError));
  }

  // GetStreamUriResponse 찾기
  auto response_node = doc.select_node("//*[local-name()='GetStreamUriResponse']");
  if (!response_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  StreamUri stream_uri;

  // MediaUri 노드 찾기
  auto media_uri_node = nx::find_child_ignore_ns(response_node.value(), "MediaUri");
  if (!media_uri_node) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }

  // Uri 파싱
  auto uri = nx::get_child_text(*media_uri_node, "Uri");
  if (!uri) {
    return std::unexpected(make_error_code(OnvifError::kInvalidResponse));
  }
  stream_uri.uri = *uri;

  // InvalidAfterConnect (optional)
  auto invalid_connect = nx::get_child_bool(*media_uri_node, "InvalidAfterConnect");
  if (invalid_connect) {
    stream_uri.invalid_after_connect = *invalid_connect;
  }

  // InvalidAfterReboot (optional)
  auto invalid_reboot = nx::get_child_bool(*media_uri_node, "InvalidAfterReboot");
  if (invalid_reboot) {
    stream_uri.invalid_after_reboot = *invalid_reboot;
  }

  // Timeout (optional)
  auto timeout = nx::get_child_int(*media_uri_node, "Timeout");
  if (timeout) {
    stream_uri.timeout = nx::seconds(*timeout);
  }

  return stream_uri;
}

std::optional<VideoEncoderConfig>
MediaService::parse_video_encoder_config(const pugi::xml_node& node)
{
  VideoEncoderConfig config;

  // Token
  config.token = nx::get_node_attribute(node, "token");

  // Name
  auto name = nx::get_child_text(node, "Name");
  if (name) {
    config.name = *name;
  }

  // Encoding (코덱)
  auto encoding = nx::get_child_text(node, "Encoding");
  if (encoding) {
    config.codec = parse_video_codec(*encoding);
  }

  // Resolution
  auto resolution_node = nx::find_child_ignore_ns(node, "Resolution");
  if (resolution_node) {
    auto width = nx::get_child_int(*resolution_node, "Width");
    auto height = nx::get_child_int(*resolution_node, "Height");

    if (width && height) {
      config.resolution.width = *width;
      config.resolution.height = *height;
    }
  }

  // Quality
  auto quality = nx::get_child_int(node, "Quality");
  if (quality) {
    config.quality = *quality;
  }

  // RateControl
  auto rate_control_node = nx::find_child_ignore_ns(node, "RateControl");
  if (rate_control_node) {
    auto framerate = nx::get_child_int(*rate_control_node, "FrameRateLimit");
    if (framerate) {
      config.framerate = *framerate;
    }

    auto bitrate = nx::get_child_int(*rate_control_node, "BitrateLimit");
    if (bitrate) {
      config.bitrate = *bitrate;
    }
  }

  // H264 (optional)
  auto h264_node = nx::find_child_ignore_ns(node, "H264");
  if (h264_node) {
    auto gop = nx::get_child_int(*h264_node, "GovLength");
    if (gop) {
      config.gop_size = *gop;
    }

    auto profile = nx::get_child_text(*h264_node, "H264Profile");
    if (profile) {
      config.profile = *profile;
    }
  }

  return config;
}

std::optional<AudioEncoderConfig>
MediaService::parse_audio_encoder_config(const pugi::xml_node& node)
{
  AudioEncoderConfig config;

  // Token
  config.token = nx::get_node_attribute(node, "token");

  // Name
  auto name = nx::get_child_text(node, "Name");
  if (name) {
    config.name = *name;
  }

  // Encoding (코덱)
  auto encoding = nx::get_child_text(node, "Encoding");
  if (encoding) {
    config.codec = parse_audio_codec(*encoding);
  }

  // Bitrate
  auto bitrate = nx::get_child_int(node, "Bitrate");
  if (bitrate) {
    config.bitrate = *bitrate;
  }

  // SampleRate
  auto sample_rate = nx::get_child_int(node, "SampleRate");
  if (sample_rate) {
    config.sample_rate = *sample_rate;
  }

  return config;
}

std::optional<PtzConfig>
MediaService::parse_ptz_config(const pugi::xml_node& node)
{
  PtzConfig config;

  // Token
  config.token = nx::get_node_attribute(node, "token");

  // Name
  auto name = nx::get_child_text(node, "Name");
  if (name) {
    config.name = *name;
  }

  // NodeToken
  auto node_token = nx::get_child_text(node, "NodeToken");
  if (node_token) {
    config.node_token = *node_token;
  }

  return config;
}

// ============================================================================
// 프로파일 우선순위 계산
// ============================================================================

int
MediaService::calculate_profile_score(const MediaProfile& profile)
{
  if (!profile.video_encoder.has_value()) {
    return 0;
  }

  const auto& encoder = profile.video_encoder.value();
  int score = 0;

  // 1순위: 코덱 점수
  switch (encoder.codec) {
    case VideoCodec::kH264: score += kCodecScoreH264; break;
    case VideoCodec::kH265: score += kCodecScoreH265; break;
    case VideoCodec::kMjpeg: score += kCodecScoreMJPEG; break;
    case VideoCodec::kMpeg4: score += kCodecScoreMPEG4; break;
    default: break;
  }

  // 2순위: 해상도 점수 (픽셀 수 / 1000)
  int pixels = encoder.resolution.width * encoder.resolution.height;
  score += pixels / 1000; // 1920x1080 = 2073점

  // 3순위: 비트레이트 점수 (Kbps / 100)
  score += encoder.bitrate / 100000; // 4000000bps = 40점

  return score;
}

int
MediaService::calculate_resolution_bonus(
  const MediaProfile& profile, int preferred_width, int preferred_height)
{
  if (!profile.video_encoder.has_value()) {
    return 0;
  }

  const auto& encoder = profile.video_encoder.value();
  int actual_width = encoder.resolution.width;
  int actual_height = encoder.resolution.height;

  // 선호 해상도와 정확히 일치하면 높은 보너스
  if (actual_width == preferred_width && actual_height == preferred_height) {
    return 5000; // 큰 보너스 (코덱 점수 차이를 극복할 수 없을 정도)
  }

  // 거리 기반 보너스 (가까울수록 높음)
  int width_diff = std::abs(actual_width - preferred_width);
  int height_diff = std::abs(actual_height - preferred_height);
  double distance = std::sqrt(width_diff * width_diff + height_diff * height_diff);

  // 거리가 가까울수록 보너스 (최대 1000점)
  int bonus = static_cast<int>(1000.0 / (1.0 + distance / 100.0));

  return bonus;
}

} // namespace nx::net::onvif::services
