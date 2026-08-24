// media_type.h
// 생성일: 2026-02-03
// 설명: 미디어 관련 공용 타입 정의

#pragma once

#include <nxcore/util/time_util.h>

#include <cstdint>
#include <vector>

namespace nx {
namespace media {

enum class MediaType
{
  kUnknown = 0,
  kVideo = 1,
  kAudio = 2,
  kMetadata = 3
};

struct Frame
{
  MediaType type = MediaType::kUnknown;
  mstime_t timestamp = 0; // 프레임 타임스탬프 (밀리초)
  mstime_t duration = 0;  // 프레임 지속 시간 (밀리초)
  bool is_keyframe = false;
  bool encoded = true;       // true: 인코딩된 데이터, false: raw 데이터
  int32_t stream_index = -1; // 소스 스트림 인덱스 (트랙 구분용)
  std::vector<uint8_t> data; // 실제 데이터
};

} // namespace media
} // namespace nx