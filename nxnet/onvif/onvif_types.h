// 파일: onvif_types.h
// 생성일: 2026-02-17
// 설명: ONVIF 프로토콜 공통 타입 정의

#pragma once

#include <nxcore/util/time_util.h>
#include <nxcore/media/media_codec.h>

#include <string>
#include <vector>
#include <optional>

namespace nx::net::onvif {

// 비디오/오디오 코덱 타입은 nx::media 정의를 공유
using VideoCodec = nx::media::VideoCodec;
using AudioCodec = nx::media::AudioCodec;

// ============================================================================
// 날짜/시간 타입
// ============================================================================

/// ONVIF DateTime 구조체 (ISO 8601 형식)
struct DateTime
{
  int year{0};
  int month{0};     // 1-12
  int day{0};       // 1-31
  int hour{0};      // 0-23
  int minute{0};    // 0-59
  int second{0};    // 0-59
  int tz_hour{0};   // 시간대 오프셋 (시)
  int tz_minute{0}; // 시간대 오프셋 (분)

  // ISO 8601 문자열로 변환 (예: 2026-02-17T10:30:00Z)
  std::string to_iso8601() const;

  // ISO 8601 문자열에서 파싱
  static std::optional<DateTime> from_iso8601(const std::string& str);

  // 현재 UTC 시간 생성
  static DateTime now_utc();

  // std::chrono::system_clock::time_point로 변환
  std::chrono::system_clock::time_point to_time_point() const;

  // std::chrono::system_clock::time_point에서 생성
  static DateTime from_time_point(const std::chrono::system_clock::time_point& tp);
};

// ============================================================================
// 스트림 프로토콜 및 전송 타입
// ============================================================================

/// 스트림 프로토콜
enum class StreamProtocol
{
  kRtsp,    // RTSP (Real Time Streaming Protocol)
  kHttp,    // HTTP
  kUdp,     // UDP
  kTcp,     // TCP
  kRtspHttp // RTSP over HTTP
};

/// 전송 타입
enum class TransportType
{
  kUnicast,  // 유니캐스트 (1:1)
  kMulticast // 멀티캐스트 (1:N)
};

// ============================================================================
// 비디오 인코딩 정보
// ============================================================================

/// 비디오 해상도
struct VideoResolution
{
  int width{0};
  int height{0};
};

/// 비디오 인코더 설정
struct VideoEncoderConfig
{
  std::string token; // 설정 토큰
  std::string name;  // 설정 이름
  VideoCodec codec{VideoCodec::kUnknown};
  VideoResolution resolution;
  int framerate{0};    // FPS
  int bitrate{0};      // bps
  int quality{0};      // 품질 (1-100, 코덱별 의미 다름)
  int gop_size{0};     // GOP 크기 (keyframe 간격)
  std::string profile; // H264Profile (Baseline, Main, High 등)
};

// ============================================================================
// 오디오 인코딩 정보
// ============================================================================

/// 오디오 인코더 설정
struct AudioEncoderConfig
{
  std::string token; // 설정 토큰
  std::string name;  // 설정 이름
  AudioCodec codec{AudioCodec::kUnknown};
  int bitrate{0};     // bps
  int sample_rate{0}; // Hz (예: 8000, 16000, 48000)
};

// ============================================================================
// PTZ 설정
// ============================================================================

/// PTZ 설정 (선택 사항)
struct PtzConfig
{
  std::string token;      // PTZ 설정 토큰
  std::string name;       // PTZ 설정 이름
  std::string node_token; // PTZ Node 토큰
};

// ============================================================================
// 미디어 프로파일
// ============================================================================

/// ONVIF 미디어 프로파일
struct MediaProfile
{
  std::string token; // 프로파일 토큰 (GetStreamUri에 사용)
  std::string name;  // 프로파일 이름
  bool fixed{false}; // 고정 프로파일 여부

  std::optional<VideoEncoderConfig> video_encoder;
  std::optional<AudioEncoderConfig> audio_encoder;
  std::optional<PtzConfig> ptz_config;

  // 프로파일 점수 계산 (우선순위 선택용)
  // 코덱(H264>H265>MJPEG) + 해상도 + 비트레이트 기반
  int calculate_priority_score() const;
};

// ============================================================================
// ONVIF Capabilities
// ============================================================================

/// Device Capabilities
struct DeviceCapabilities
{
  std::string device_service_url; // Device Management Service (tds)
  std::string system_log_url;     // System Log (optional)
};

/// Media Capabilities
struct MediaCapabilities
{
  std::string media_service_url;   // Media Service (trt)
  bool streaming_supported{false}; // RTP 스트리밍 지원 여부
  bool snapshot_supported{false};  // 스냅샷 지원 여부
};

/// PTZ Capabilities
struct PtzCapabilities
{
  std::string ptz_service_url; // PTZ Service (tptz)
};

/// Event Capabilities
struct EventCapabilities
{
  std::string event_service_url;    // Event Service (tev)
  bool pull_point_supported{false}; // PullPoint 지원 여부
};

/// Imaging Capabilities
struct ImagingCapabilities
{
  std::string imaging_service_url; // Imaging Service (timg)
};

/// ONVIF GetCapabilities 응답
struct OnvifCapabilities
{
  DeviceCapabilities device;
  MediaCapabilities media;
  std::optional<PtzCapabilities> ptz;
  std::optional<EventCapabilities> events;
  std::optional<ImagingCapabilities> imaging;
};

// ============================================================================
// 장비 정보
// ============================================================================

/// ONVIF GetDeviceInformation 응답
struct DeviceInfo
{
  std::string manufacturer;     // 제조사
  std::string model;            // 모델명
  std::string firmware_version; // 펌웨어 버전
  std::string serial_number;    // 시리얼 번호
  std::string hardware_id;      // 하드웨어 ID
};

// ============================================================================
// 스트림 URI 정보
// ============================================================================

/// GetStreamUri 응답
struct StreamUri
{
  std::string uri;                   // RTSP URI (예: rtsp://192.168.0.168:554/stream1)
  bool invalid_after_connect{false}; // 연결 후 무효화 여부
  bool invalid_after_reboot{false};  // 재부팅 후 무효화 여부
  nx::seconds timeout{0};            // URI 유효 시간
};

// ============================================================================
// 네트워크 인터페이스 정보
// ============================================================================

/// 네트워크 인터페이스
struct NetworkInterface
{
  std::string token;         // 인터페이스 토큰
  bool enabled{false};       // 활성화 여부
  std::string ipv4_address;  // IPv4 주소
  int ipv4_prefix_length{0}; // 서브넷 마스크 (CIDR)
  std::string mac_address;   // MAC 주소
};

} // namespace nx::net::onvif
