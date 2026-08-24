// 파일: media_error.h
// 생성일: 2026-02-26
// 설명: 미디어 파이프라인 범용 에러 코드

#pragma once

#include <system_error>
#include <string>

namespace nx {
namespace media {

/// 미디어 파이프라인 에러 코드
enum class MediaErrc
{
  kSuccess = 0,
  kSourceOpenFailed = 1,   // 소스 연결 실패
  kSourceNoTracks = 2,     // 소스에서 트랙 정보 없음
  kSinkOpenFailed = 3,     // 싱크 초기화 실패
  kSourceStartFailed = 4,  // 소스 시작 실패
  kPipelineNotRunning = 5, // 파이프라인 미실행 상태
  kUnsupportedCodec = 6,   // 지원하지 않는 코덱
  kTimeout = 7             // 작업 타임아웃
};

/// MediaErrc 에러 카테고리
class MediaErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "media"; }

  std::string message(int ev) const override
  {
    switch (static_cast<MediaErrc>(ev)) {
      case MediaErrc::kSuccess: return "success";
      case MediaErrc::kSourceOpenFailed: return "media source open failed";
      case MediaErrc::kSourceNoTracks: return "media source returned no tracks";
      case MediaErrc::kSinkOpenFailed: return "media sink open failed";
      case MediaErrc::kSourceStartFailed: return "media source start failed";
      case MediaErrc::kPipelineNotRunning: return "media pipeline not running";
      case MediaErrc::kUnsupportedCodec: return "unsupported codec";
      case MediaErrc::kTimeout: return "operation timed out";
      default: return "unknown media error (" + std::to_string(ev) + ")";
    }
  }
};

inline const MediaErrorCategory&
media_error_category()
{
  static MediaErrorCategory instance;
  return instance;
}

inline std::error_code
make_error_code(MediaErrc e)
{
  return {static_cast<int>(e), media_error_category()};
}

} // namespace media
} // namespace nx

template <>
struct std::is_error_code_enum<nx::media::MediaErrc> : std::true_type
{};
