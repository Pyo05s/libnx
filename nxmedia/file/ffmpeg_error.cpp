// 파일: ffmpeg_error.cpp
// 생성일: 2026-02-06
// 설명: FFmpeg 오류 코드 변환 구현

#include "ffmpeg_error.h"

extern "C" {
#include <libavutil/error.h>
}

namespace nx {
namespace media {

const char*
FfmpegErrorCategory::name() const noexcept
{
  return "ffmpeg";
}

std::string
FfmpegErrorCategory::message(int ev) const
{
  switch (static_cast<FfmpegError>(ev)) {
    case FfmpegError::kSuccess: return "Success";
    case FfmpegError::kFileNotFound: return "File not found";
    case FfmpegError::kInvalidFormat: return "Invalid format";
    case FfmpegError::kStreamNotFound: return "Stream not found";
    case FfmpegError::kEndOfFile: return "End of file";
    case FfmpegError::kSeekFailed: return "Seek failed";
    case FfmpegError::kReadFailed: return "Read failed";
    case FfmpegError::kInvalidState: return "Invalid state";
    case FfmpegError::kUnknownError:
    default: return "Unknown error";
  }
}

const std::error_category&
ffmpeg_category() noexcept
{
  static FfmpegErrorCategory instance;
  return instance;
}

std::error_code
make_error_code(FfmpegError e) noexcept
{
  return {static_cast<int>(e), ffmpeg_category()};
}

std::error_code
from_av_error(int av_error) noexcept
{
  if (av_error >= 0) {
    return {};
  }

  // FFmpeg 오류 코드를 FfmpegError로 매핑
  switch (av_error) {
    case AVERROR_EOF: return make_error_code(FfmpegError::kEndOfFile);

    case AVERROR(ENOENT): return make_error_code(FfmpegError::kFileNotFound);

    case AVERROR_INVALIDDATA:
    case AVERROR_STREAM_NOT_FOUND:
      return make_error_code(FfmpegError::kStreamNotFound);

    default: return make_error_code(FfmpegError::kUnknownError);
  }
}

} // namespace media
} // namespace nx
