// 파일: media_codec.h
// 생성일: 2026-02-26
// 설명: 미디어 코덱 타입 정의 (비디오/오디오)

#pragma once

#include <cstdint>
#include <string_view>

#include <nxcore/util/string_util.h>

namespace nx {
namespace media {

/// 비디오 코덱 타입
enum class VideoCodec : uint8_t
{
  kUnknown = 0,
  kH264 = 1,
  kH265 = 2,
  kMjpeg = 3,
  kMpeg4 = 4 // MPEG-4
};

/// 오디오 코덱 타입
enum class AudioCodec : uint8_t
{
  kUnknown = 0,
  kAac = 1,      // AAC-LC
  kG711Alaw = 2, // G.711 A-law (PCMA)
  kG711Ulaw = 3, // G.711 μ-law (PCMU)
  kPcm = 4,      // Raw PCM
  kG711 = 5,     // G.711 (A-law/μ-law 미구분, ONVIF 호환)
  kG726 = 6      // G.726
};

// ============================================================================
// 코덱 ↔ 문자열 변환
// ============================================================================

/// VideoCodec → 문자열 (소문자)
constexpr std::string_view
video_codec_to_string(VideoCodec codec)
{
  switch (codec) {
    case VideoCodec::kH264: return "h264";
    case VideoCodec::kH265: return "h265";
    case VideoCodec::kMjpeg: return "mjpeg";
    case VideoCodec::kMpeg4: return "mpeg4";
    default: return "unknown";
  }
}

/// AudioCodec → 문자열 (소문자)
constexpr std::string_view
audio_codec_to_string(AudioCodec codec)
{
  switch (codec) {
    case AudioCodec::kAac: return "aac";
    case AudioCodec::kG711Alaw: return "g711a";
    case AudioCodec::kG711Ulaw: return "g711u";
    case AudioCodec::kG711: return "g711";
    case AudioCodec::kG726: return "g726";
    case AudioCodec::kPcm: return "pcm";
    default: return "unknown";
  }
}

/// 문자열 → VideoCodec (대소문자 무시)
constexpr VideoCodec
video_codec_from_string(std::string_view name)
{
  if (iequals(name, "h264"))
    return VideoCodec::kH264;
  if (iequals(name, "h265") || iequals(name, "hevc"))
    return VideoCodec::kH265;
  if (iequals(name, "mjpeg") || iequals(name, "jpeg"))
    return VideoCodec::kMjpeg;
  if (iequals(name, "mpeg4"))
    return VideoCodec::kMpeg4;
  return VideoCodec::kUnknown;
}

/// 문자열 → AudioCodec (대소문자 무시)
constexpr AudioCodec
audio_codec_from_string(std::string_view name)
{
  if (iequals(name, "aac"))
    return AudioCodec::kAac;
  if (iequals(name, "g711a"))
    return AudioCodec::kG711Alaw;
  if (iequals(name, "g711u"))
    return AudioCodec::kG711Ulaw;
  if (iequals(name, "g711"))
    return AudioCodec::kG711;
  if (iequals(name, "g726"))
    return AudioCodec::kG726;
  if (iequals(name, "pcm"))
    return AudioCodec::kPcm;
  return AudioCodec::kUnknown;
}

} // namespace media
} // namespace nx
