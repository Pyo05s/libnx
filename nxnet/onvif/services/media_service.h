// 파일: media_service.h
// 생성일: 2026-02-19
// 설명: ONVIF Media Service

#pragma once

#include "nxnet/onvif/soap/soap_client.h"
#include "nxnet/onvif/onvif_types.h"
#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <expected>
#include <memory>
#include <string>
#include <vector>

// pugixml 전방 선언
namespace pugi {
class xml_node;
}

namespace nx::net::onvif::services {

// ============================================================================
// 스트림 용도
// ============================================================================

/// 스트림 사용 목적
enum class StreamPurpose
{
  kMain,  // 녹화용 (고해상도 우선)
  kSecond // 라이브용 (저해상도 우선)
};

// ============================================================================
// 프로파일 선택 옵션
// ============================================================================

/// 프로파일 선택 옵션
struct ProfileSelectionOptions
{
  StreamPurpose purpose{StreamPurpose::kMain}; // 스트림 용도

  // 해상도 선호도 (용도에 따라 자동 설정되지만 수동 지정 가능)
  int preferred_width{0};  // 0이면 자동 (Main: 1920, Second: 1280)
  int preferred_height{0}; // 0이면 자동 (Main: 1080, Second: 720)
};

// ============================================================================
// Media Service
// ============================================================================

/// ONVIF Media Service (trt)
/// 미디어 프로파일, 스트림 URI 조회 등을 담당
class MediaService
{
public:
  /// 생성자
  /// @param soap_client SOAP 클라이언트 (공유)
  /// @param service_url Media Service URL
  explicit MediaService(
    std::shared_ptr<soap::SoapClient> soap_client, std::string service_url);

  ~MediaService();

  NX_NON_COPYABLE_AND_MOVABLE(MediaService);

  // ========================================================================
  // ONVIF Media Service 메서드
  // ========================================================================

  /// GetProfiles
  /// 미디어 프로파일 목록 조회
  /// @return 프로파일 목록
  nx::awaitable_expected<std::vector<MediaProfile>> get_profiles();

  /// GetStreamUri
  /// 특정 프로파일의 스트림 URI 조회
  /// @param profile_token 프로파일 토큰
  /// @param protocol 스트림 프로토콜 (기본: RTSP)
  /// @param transport 전송 타입 (기본: Unicast)
  /// @return 스트림 URI 정보
  nx::awaitable_expected<StreamUri> get_stream_uri(
    const std::string& profile_token,
    StreamProtocol protocol = StreamProtocol::kRtsp,
    TransportType transport = TransportType::kUnicast);

  // ========================================================================
  // 프로파일 선택 헬퍼
  // ========================================================================

  /// 용도에 맞는 최적 프로파일 선택
  /// @param profiles 프로파일 목록
  /// @param options 선택 옵션
  /// @return 선택된 프로파일 (없으면 nullopt)
  static std::optional<MediaProfile> select_best_profile(
    const std::vector<MediaProfile>& profiles, const ProfileSelectionOptions& options);

  /// Main 스트림용 프로파일 선택 (녹화용)
  static std::optional<MediaProfile>
  select_main_profile(const std::vector<MediaProfile>& profiles);

  /// Second 스트림용 프로파일 선택 (라이브용)
  static std::optional<MediaProfile>
  select_second_profile(const std::vector<MediaProfile>& profiles);

  // ========================================================================
  // 서비스 URL 관리
  // ========================================================================

  /// 서비스 URL 조회
  std::string get_service_url() const;

  /// 서비스 URL 변경
  void set_service_url(const std::string& url);

private:
  // ========================================================================
  // 내부 파싱 메서드
  // ========================================================================

  /// GetProfiles 응답 파싱
  nx::expected<std::vector<MediaProfile>>
  parse_profiles_response(const std::string& response_body);

  /// GetStreamUri 응답 파싱
  nx::expected<StreamUri> parse_stream_uri_response(const std::string& response_body);

  /// VideoEncoderConfiguration 파싱
  std::optional<VideoEncoderConfig>
  parse_video_encoder_config(const pugi::xml_node& node);

  /// AudioEncoderConfiguration 파싱
  std::optional<AudioEncoderConfig>
  parse_audio_encoder_config(const pugi::xml_node& node);

  /// PTZConfiguration 파싱
  std::optional<PtzConfig> parse_ptz_config(const pugi::xml_node& node);

  // ========================================================================
  // 프로파일 우선순위 계산
  // ========================================================================

  /// 프로파일 우선순위 점수 계산
  /// 코덱(1순위) > 해상도(2순위) > 비트레이트(3순위)
  static int calculate_profile_score(const MediaProfile& profile);

  /// 해상도 선호도에 따른 보너스 점수 계산
  static int calculate_resolution_bonus(
    const MediaProfile& profile, int preferred_width, int preferred_height);

private:
  // ========================================================================
  // 멤버 변수
  // ========================================================================

  std::shared_ptr<soap::SoapClient> m_soap_client;
  std::string m_service_url;
};

} // namespace nx::net::onvif::services
