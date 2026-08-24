// 파일: media_frame.h
// 생성일: 2026-02-26
// 설명: 미디어 프레임 - 파이프라인 내부 전송 단위

#pragma once

#include "media_codec.h"
#include "media_type.h"
#include "../util/time_util.h"

#include <cstdint>
#include <memory>
#include <vector>

namespace nx {
namespace media {

/// 미디어 프레임 (파이프라인 내부 전송 단위)
/// Frame 구조체를 확장하여 코덱/해상도/오디오 파라미터 포함
struct MediaFrame
{
  MediaType type = MediaType::kUnknown;
  mstime_t timestamp = 0;    // 프레임 타임스탬프 (밀리초, 소스별 프레젠테이션 시간)
  mstime_t capture_time = 0; // 프레임 캡처/수신 시각 (UTC wall clock, 밀리초)
  mstime_t duration = 0;     // 프레임 지속 시간 (밀리초)
  bool is_keyframe = false;
  int32_t stream_index = -1; // 트랙 인덱스

  // 코덱 정보 (첫 프레임 또는 코덱 변경 시 유효)
  VideoCodec video_codec = VideoCodec::kUnknown;
  AudioCodec audio_codec = AudioCodec::kUnknown;

  // 코덱 구성 데이터 (SPS/PPS, AudioSpecificConfig 등)
  std::vector<uint8_t> codec_config;

  // 실제 인코딩 데이터 (shared_ptr 기반 zero-copy 전달, 기본값 nullptr)
  std::shared_ptr<std::vector<uint8_t>> data = nullptr;

  // 비디오 해상도 (키프레임에서 업데이트)
  uint32_t width = 0;
  uint32_t height = 0;

  // 오디오 파라미터
  uint32_t sample_rate = 0;
  uint16_t channels = 0;

  // 스트림 종료 마커 (소스가 모든 데이터를 전송 완료했음을 알림)
  bool is_eof = false;
};

} // namespace media
} // namespace nx
