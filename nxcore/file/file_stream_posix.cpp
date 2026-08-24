// 파일: file_stream_posix.cpp
// 생성일: 2026-05-22
// 설명: POSIX 네이티브 파일 스트림 구현 (open/read/write/lseek 기반)

#include "file_stream.h"

#ifndef _WIN32

#include <cerrno>
#include <fcntl.h>
#include <unistd.h>

namespace nx::file {

// ─── 생성자 / 소멸자 ───

FileStream::FileStream() noexcept
    : m_fd(-1)
{}

FileStream::~FileStream()
{
  close();
}

// ─── 이동 연산 ───

FileStream::FileStream(FileStream&& other) noexcept
    : m_fd(other.m_fd)
{
  other.m_fd = -1;
}

FileStream&
FileStream::operator=(FileStream&& other) noexcept
{
  if (this != &other) {
    close();
    m_fd = other.m_fd;
    other.m_fd = -1;
  }
  return *this;
}

// ─── is_open ───

bool
FileStream::is_open() const noexcept
{
  return m_fd >= 0;
}

// ─── open ───

std::error_code
FileStream::open(const std::filesystem::path& path, OpenMode mode)
{
  if (is_open()) {
    return make_error_code(FileErrc::kAlreadyOpen);
  }

  int flags = 0;

  switch (mode) {
    case OpenMode::kRead: flags = O_RDONLY; break;
    case OpenMode::kWriteTruncate: flags = O_WRONLY | O_CREAT | O_TRUNC; break;
    case OpenMode::kAppend: flags = O_WRONLY | O_CREAT | O_APPEND; break;
  }

  // 생성 시 권한: rw-r--r--
  constexpr mode_t kFileMode = S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH;

  m_fd = ::open(path.c_str(), flags, kFileMode);
  if (m_fd < 0) {
    return make_error_code(FileErrc::kOpenFailed);
  }

  return {};
}

// ─── close ───

void
FileStream::close()
{
  if (is_open()) {
    ::close(m_fd);
    m_fd = -1;
  }
}

// ─── write ───

nx::expected<std::size_t>
FileStream::write(std::span<const uint8_t> data)
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  ssize_t written = ::write(m_fd, data.data(), data.size());
  if (written < 0) {
    return std::unexpected(make_error_code(FileErrc::kWriteFailed));
  }
  return static_cast<std::size_t>(written);
}

// ─── read ───

nx::expected<std::size_t>
FileStream::read(std::span<uint8_t> buffer)
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  ssize_t bytes_read = ::read(m_fd, buffer.data(), buffer.size());
  if (bytes_read < 0) {
    return std::unexpected(make_error_code(FileErrc::kReadFailed));
  }
  return static_cast<std::size_t>(bytes_read);
}

// ─── seek ───

nx::expected<int64_t>
FileStream::seek(int64_t offset, SeekOrigin origin)
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  int whence = SEEK_SET;
  switch (origin) {
    case SeekOrigin::kBegin: whence = SEEK_SET; break;
    case SeekOrigin::kCurrent: whence = SEEK_CUR; break;
    case SeekOrigin::kEnd: whence = SEEK_END; break;
  }

  off_t new_pos = ::lseek(m_fd, static_cast<off_t>(offset), whence);
  if (new_pos == static_cast<off_t>(-1)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }
  return static_cast<int64_t>(new_pos);
}

// ─── tell ───

nx::expected<int64_t>
FileStream::tell() const
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  off_t pos = ::lseek(m_fd, 0, SEEK_CUR);
  if (pos == static_cast<off_t>(-1)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }
  return static_cast<int64_t>(pos);
}

// ─── flush ───

std::error_code
FileStream::flush()
{
  if (!is_open()) {
    return make_error_code(FileErrc::kNotOpen);
  }

  if (::fsync(m_fd) != 0) {
    return make_error_code(FileErrc::kFlushFailed);
  }
  return {};
}

// ─── file_size ───

nx::expected<int64_t>
FileStream::file_size() const
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  // 현재 위치 저장 후 파일 끝으로 이동해 크기 측정, 복원
  off_t saved = ::lseek(m_fd, 0, SEEK_CUR);
  if (saved == static_cast<off_t>(-1)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }

  off_t size = ::lseek(m_fd, 0, SEEK_END);
  if (size == static_cast<off_t>(-1)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }

  // 원래 위치로 복원 (실패해도 크기 결과는 유효)
  ::lseek(m_fd, saved, SEEK_SET);

  return static_cast<int64_t>(size);
}

// ─── file_error_category 구현 ───

namespace {

class FileErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "nx.file"; }

  std::string message(int ev) const override
  {
    switch (static_cast<FileErrc>(ev)) {
      case FileErrc::kSuccess: return "success";
      case FileErrc::kOpenFailed: return "file open failed";
      case FileErrc::kReadFailed: return "file read failed";
      case FileErrc::kWriteFailed: return "file write failed";
      case FileErrc::kSeekFailed: return "file seek failed";
      case FileErrc::kFlushFailed: return "file flush failed";
      case FileErrc::kNotOpen: return "file is not open";
      case FileErrc::kAlreadyOpen: return "file is already open";
      default: return "unknown file error";
    }
  }
};

} // namespace

const std::error_category&
file_error_category() noexcept
{
  static const FileErrorCategory kCategory;
  return kCategory;
}

} // namespace nx::file

#endif // !_WIN32
