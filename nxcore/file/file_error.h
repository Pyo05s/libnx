// 파일: file_error.h
// 생성일: 2026-05-22
// 설명: 네이티브 파일 I/O 에러 코드 정의

#pragma once

#include <system_error>

namespace nx::file {

/// 파일 I/O 에러 코드
enum class FileErrc
{
  kSuccess = 0,
  kOpenFailed,  // 파일 열기 실패
  kReadFailed,  // 읽기 실패
  kWriteFailed, // 쓰기 실패
  kSeekFailed,  // 탐색 실패
  kFlushFailed, // 플러시/동기화 실패
  kNotOpen,     // 파일이 열려있지 않음
  kAlreadyOpen, // 이미 열려있는 상태에서 open 시도
};

/// std::error_code 통합
const std::error_category& file_error_category() noexcept;

inline std::error_code
make_error_code(FileErrc e) noexcept
{
  return {static_cast<int>(e), file_error_category()};
}

} // namespace nx::file

// std::error_code 변환 등록
template <>
struct std::is_error_code_enum<nx::file::FileErrc> : std::true_type
{};
