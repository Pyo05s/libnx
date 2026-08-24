// 파일: record_error.h
// 생성일: 2025-12-03
// 설명: 녹화 시스템 오류 코드 정의

#pragma once

#include <system_error>
#include <string>

namespace nx {
namespace record {

// 녹화 시스템 오류 코드
enum class RecordErrc
{
  // 성공
  success = 0,

  // 입력 검증 오류
  invalid_block = 1,     // 잘못된 블록 데이터
  empty_block = 2,       // 빈 블록
  invalid_timestamp = 3, // 잘못된 타임스탬프

  // 파일 시스템 오류
  file_open_failed = 10,        // 파일 열기 실패
  file_write_failed = 11,       // 파일 쓰기 실패
  file_sync_failed = 12,        // 파일 동기화 실패
  directory_create_failed = 13, // 디렉토리 생성 실패
  file_already_exists = 14,     // 파일이 이미 존재함

  // 리소스 오류
  no_space_on_device = 20,   // 디스크 공간 부족
  permission_denied = 21,    // 권한 거부
  read_only_filesystem = 22, // 읽기 전용 파일 시스템

  // 세그먼트 관리 오류
  segment_not_open = 30,        // 세그먼트가 열려있지 않음
  segment_rotation_failed = 31, // 세그먼트 전환 실패
  header_write_failed = 32,     // 헤더 쓰기 실패
  footer_write_failed = 33,     // 푸터 쓰기 실패

  // 내부 오류
  internal_error = 100, // 내부 오류
};

// 오류 코드 카테고리
class RecordErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx::record"; }

  std::string message(int ev) const override
  {
    switch (static_cast<RecordErrc>(ev)) {
      case RecordErrc::success: return "Success";

      // 입력 검증 오류
      case RecordErrc::invalid_block: return "Invalid block data";
      case RecordErrc::empty_block: return "Empty block";
      case RecordErrc::invalid_timestamp: return "Invalid timestamp";

      // 파일 시스템 오류
      case RecordErrc::file_open_failed: return "Failed to open file";
      case RecordErrc::file_write_failed: return "Failed to write to file";
      case RecordErrc::file_sync_failed: return "Failed to sync file to disk";
      case RecordErrc::directory_create_failed: return "Failed to create directory";
      case RecordErrc::file_already_exists: return "File already exists";

      // 리소스 오류
      case RecordErrc::no_space_on_device: return "No space left on device";
      case RecordErrc::permission_denied: return "Permission denied";
      case RecordErrc::read_only_filesystem: return "Read-only file system";

      // 세그먼트 관리 오류
      case RecordErrc::segment_not_open: return "Segment is not open";
      case RecordErrc::segment_rotation_failed: return "Failed to rotate segment";
      case RecordErrc::header_write_failed: return "Failed to write segment header";
      case RecordErrc::footer_write_failed: return "Failed to write segment footer";

      // 내부 오류
      case RecordErrc::internal_error: return "Internal error";

      default: return "Unknown error";
    }
  }

  // 오류가 영구적인지 판단 (재시도 불가능)
  bool is_permanent_error(int ev) const noexcept
  {
    switch (static_cast<RecordErrc>(ev)) {
      case RecordErrc::no_space_on_device:
      case RecordErrc::permission_denied:
      case RecordErrc::read_only_filesystem:
      case RecordErrc::invalid_block:
      case RecordErrc::empty_block:
      case RecordErrc::invalid_timestamp: return true;
      default: return false;
    }
  }
};

// 싱글톤 카테고리 인스턴스
inline const RecordErrorCategory&
record_error_category()
{
  static RecordErrorCategory instance;
  return instance;
}

// error_code 생성 헬퍼
inline std::error_code
make_error_code(RecordErrc e)
{
  return {static_cast<int>(e), record_error_category()};
}

// is_permanent_error 헬퍼 함수
inline bool
is_permanent_error(const std::error_code& ec)
{
  if (ec.category() == record_error_category()) {
    return static_cast<const RecordErrorCategory&>(ec.category())
      .is_permanent_error(ec.value());
  }

  // 시스템 오류 중 영구적인 것들
  if (ec.category() == std::system_category()) {
    switch (static_cast<std::errc>(ec.value())) {
      case std::errc::no_space_on_device:
      case std::errc::permission_denied:
      case std::errc::read_only_file_system:
      case std::errc::invalid_argument:
      case std::errc::not_supported: return true;
      default: break;
    }
  }

  return false;
}

} // namespace record
} // namespace nx

// std::error_code와 통합
namespace std {
template <>
struct is_error_code_enum<nx::record::RecordErrc> : true_type
{};
} // namespace std
