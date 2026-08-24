// 파일: media_source.h
// 생성일: 2026-02-26
// 설명: 미디어 소스 인터페이스 - 파이프라인 데이터 발생 원점 (공용 라이브러리)

#pragma once

#include "media_codec.h"
#include "media_frame.h"
#include "media_type.h"
#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <cstdint>
#include <expected>
#include <functional>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nx {
namespace media {

/// 미디어 트랙 정보 (소스에서 제공하는 스트림 정보)
struct MediaTrackInfo
{
  int32_t track_index = -1;
  MediaType type = MediaType::kUnknown;

  // 코덱
  VideoCodec video_codec = VideoCodec::kUnknown;
  AudioCodec audio_codec = AudioCodec::kUnknown;

  // 비디오 속성
  uint32_t width = 0;
  uint32_t height = 0;
  double framerate = 0.0;

  // 오디오 속성
  uint32_t sample_rate = 0;
  uint16_t channels = 0;

  // RTP 페이로드 타입 (SDP rtpmap에서 추출)
  uint8_t rtp_payload_type = 0;

  // 코덱 구성 데이터 (SPS/PPS, AudioSpecificConfig 등)
  std::vector<uint8_t> codec_config;

  // RTSP control URL (세션별)
  std::string control_url;

  // fmtp 원문
  std::string fmtp;

  // SDP 원본 m= 블록 (비표준 미디어 패스스루용)
  // 카메라에서 수신한 SDP 미디어 라인을 그대로 보존하여 하류 클라이언트에 전달
  std::string sdp_media_block;
};

/// 프레임 콜백 (소스 → 파이프라인)
using FrameCallback = std::function<void(MediaFrame)>;

/// 미디어 소스 순수 가상 인터페이스
/// - RTSP 클라이언트, 파일 리더 등의 기반 클래스
/// - 비동기 연결/미디어 정보 조회/프레임 수신
class IMediaSource
{
public:
  virtual ~IMediaSource() = default;

  /// 소스 연결 및 미디어 정보 조회
  /// @return 트랙 정보 목록 또는 에러
  [[nodiscard]]
  virtual nx::awaitable_expected<std::vector<MediaTrackInfo>> open() = 0;

  /// 프레임 수신 시작 (콜백으로 프레임 전달)
  /// @param callback 프레임 수신 콜백
  [[nodiscard]]
  virtual nx::awaitable<std::error_code> start(FrameCallback callback) = 0;

  /// 소스 중지 및 리소스 해제
  [[nodiscard]]
  virtual nx::awaitable<void> close() = 0;

  /// 소스 이름 (로깅용)
  virtual std::string_view source_name() const = 0;

  /// 소스 URL 또는 경로
  virtual std::string source_url() const = 0;

  /// 트랙 정보 (open 후 유효)
  virtual const std::vector<MediaTrackInfo>& tracks() const = 0;
};

} // namespace media
} // namespace nx
