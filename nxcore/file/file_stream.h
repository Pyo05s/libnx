// 파일: file_stream.h
// 생성일: 2026-05-22
// 설명: CRT lock 우회 네이티브 파일 스트림

#pragma once

#include "file_error.h"

#include <nxcore/util/type_util.h>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <span>
#include <system_error>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif

namespace nx::file {

/// 파일 열기 모드
enum class OpenMode
{
  kRead,          // 읽기 전용 (파일 존재해야 함)
  kWriteTruncate, // 쓰기 전용, 기존 내용 삭제 (없으면 생성)
  kAppend,        // 쓰기 전용, 기존 내용 뒤에 추가 (없으면 생성)
};

/// seek 기준점
enum class SeekOrigin
{
  kBegin,   // 파일 시작
  kCurrent, // 현재 위치
  kEnd,     // 파일 끝
};

/// CRT 전역 lock을 우회하는 네이티브 파일 스트림
/// Windows: CreateFileW/ReadFile/WriteFile 직접 사용
/// POSIX: open/read/write/lseek 직접 사용
class FileStream
{
  NX_NON_COPYABLE(FileStream);

public:
  FileStream() noexcept;
  ~FileStream();

  // 이동 가능
  FileStream(FileStream&& other) noexcept;
  FileStream& operator=(FileStream&& other) noexcept;

  /// 파일 열기
  /// @param path 파일 경로
  /// @param mode 열기 모드
  /// @return 에러 코드 (성공 시 빈 error_code)
  std::error_code open(const std::filesystem::path& path, OpenMode mode);

  /// 파일 닫기
  void close();

  /// 열려있는지 확인
  bool is_open() const noexcept;

  /// 데이터 쓰기
  /// @param data 쓸 데이터
  /// @return 실제 기록된 바이트 수 또는 에러
  nx::expected<std::size_t> write(std::span<const uint8_t> data);

  /// 데이터 읽기
  /// @param buffer 읽을 버퍼
  /// @return 실제 읽은 바이트 수 또는 에러 (0 = EOF)
  nx::expected<std::size_t> read(std::span<uint8_t> buffer);

  /// 위치 이동
  /// @param offset 바이트 오프셋
  /// @param origin 기준점
  /// @return 이동 후 절대 위치 또는 에러
  nx::expected<int64_t> seek(int64_t offset, SeekOrigin origin);

  /// 현재 위치 조회
  nx::expected<int64_t> tell() const;

  /// 버퍼 플러시 + OS 동기화 (FlushFileBuffers / fsync)
  std::error_code flush();

  /// 파일 크기 조회
  nx::expected<int64_t> file_size() const;

  // ─── 편의 메서드 (헤더에 inline 구현) ───

  /// 고정 크기 구조체 쓰기
  template <typename T>
  std::error_code write_struct(const T& value)
  {
    auto result = write({reinterpret_cast<const uint8_t*>(&value), sizeof(T)});
    if (!result)
      return result.error();
    if (*result != sizeof(T))
      return make_error_code(FileErrc::kWriteFailed);
    return {};
  }

  /// 고정 크기 구조체 읽기
  template <typename T>
  bool read_struct(T& out)
  {
    auto result = read({reinterpret_cast<uint8_t*>(&out), sizeof(T)});
    return result.has_value() && *result == sizeof(T);
  }

  /// char 포인터 + 크기 기반 쓰기 (기존 코드 호환용)
  nx::expected<std::size_t> write(const char* data, std::size_t size)
  {
    return write({reinterpret_cast<const uint8_t*>(data), size});
  }

  /// char 포인터 + 크기 기반 읽기 (기존 코드 호환용)
  nx::expected<std::size_t> read(char* buffer, std::size_t size)
  {
    return read({reinterpret_cast<uint8_t*>(buffer), size});
  }

private:
#ifdef _WIN32
  HANDLE m_handle = INVALID_HANDLE_VALUE;
#else
  int m_fd = -1;
#endif
};

} // namespace nx::file
