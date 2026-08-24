// 파일: ffmpeg_error.h
// 생성일: 2026-02-06
// 설명: FFmpeg 오류 코드를 std::error_code로 변환하기 위한 유틸리티

#pragma once

#include <system_error>

namespace nx {
namespace media {

// FFmpeg 오류 카테고리
enum class FfmpegError
{
  kSuccess = 0,
  kFileNotFound,
  kInvalidFormat,
  kStreamNotFound,
  kEndOfFile,
  kSeekFailed,
  kReadFailed,
  kInvalidState,
  kUnknownError
};

// FFmpeg 오류 카테고리 클래스
class FfmpegErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override;
  std::string message(int ev) const override;
};

// 전역 카테고리 인스턴스
const std::error_category& ffmpeg_category() noexcept;

// FfmpegError를 error_code로 변환
std::error_code make_error_code(FfmpegError e) noexcept;

// FFmpeg native error code를 error_code로 변환
std::error_code from_av_error(int av_error) noexcept;

} // namespace media
} // namespace nx

// std::error_code와 통합
namespace std {
template <>
struct is_error_code_enum<nx::media::FfmpegError> : true_type
{};
} // namespace std
