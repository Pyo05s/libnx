// 파일: file_stream_win32.cpp
// 생성일: 2026-05-22
// 설명: Windows 네이티브 파일 스트림 구현 (CreateFileW 기반)

#include "file_stream.h"

#ifdef _WIN32

namespace nx::file {

// ─── 생성자 / 소멸자 ───

FileStream::FileStream() noexcept
    : m_handle(INVALID_HANDLE_VALUE)
{}

FileStream::~FileStream()
{
  close();
}

// ─── 이동 연산 ───

FileStream::FileStream(FileStream&& other) noexcept
    : m_handle(other.m_handle)
{
  other.m_handle = INVALID_HANDLE_VALUE;
}

FileStream&
FileStream::operator=(FileStream&& other) noexcept
{
  if (this != &other) {
    close();
    m_handle = other.m_handle;
    other.m_handle = INVALID_HANDLE_VALUE;
  }
  return *this;
}

// ─── is_open ───

bool
FileStream::is_open() const noexcept
{
  return m_handle != INVALID_HANDLE_VALUE;
}

// ─── open ───

std::error_code
FileStream::open(const std::filesystem::path& path, OpenMode mode)
{
  if (is_open()) {
    return make_error_code(FileErrc::kAlreadyOpen);
  }

  DWORD desired_access = 0;
  DWORD creation_disposition = 0;
  DWORD share_mode = FILE_SHARE_READ; // 기본: 읽기 공유 허용

  switch (mode) {
    case OpenMode::kRead:
      desired_access = GENERIC_READ;
      creation_disposition = OPEN_EXISTING;
      share_mode = FILE_SHARE_READ | FILE_SHARE_WRITE; // Reader는 공유 확대
      break;
    case OpenMode::kWriteTruncate:
      desired_access = GENERIC_WRITE;
      creation_disposition = CREATE_ALWAYS;
      break;
    case OpenMode::kAppend:
      desired_access = GENERIC_WRITE;
      creation_disposition = OPEN_ALWAYS;
      break;
  }

  m_handle = CreateFileW(
    path.c_str(),
    desired_access,
    share_mode,
    nullptr,
    creation_disposition,
    FILE_ATTRIBUTE_NORMAL,
    nullptr);

  if (m_handle == INVALID_HANDLE_VALUE) {
    return make_error_code(FileErrc::kOpenFailed);
  }

  // Append 모드: 파일 끝으로 이동
  if (mode == OpenMode::kAppend) {
    LARGE_INTEGER zero{};
    SetFilePointerEx(m_handle, zero, nullptr, FILE_END);
  }

  return {};
}

// ─── close ───

void
FileStream::close()
{
  if (is_open()) {
    CloseHandle(m_handle);
    m_handle = INVALID_HANDLE_VALUE;
  }
}

// ─── write ───

nx::expected<std::size_t>
FileStream::write(std::span<const uint8_t> data)
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  DWORD written = 0;
  if (!WriteFile(
        m_handle,
        data.data(),
        static_cast<DWORD>(data.size()),
        &written,
        nullptr)) {
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

  DWORD bytes_read = 0;
  if (!ReadFile(
        m_handle,
        buffer.data(),
        static_cast<DWORD>(buffer.size()),
        &bytes_read,
        nullptr)) {
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

  DWORD method = FILE_BEGIN;
  switch (origin) {
    case SeekOrigin::kBegin: method = FILE_BEGIN; break;
    case SeekOrigin::kCurrent: method = FILE_CURRENT; break;
    case SeekOrigin::kEnd: method = FILE_END; break;
  }

  LARGE_INTEGER li;
  li.QuadPart = offset;
  LARGE_INTEGER new_pos{};

  if (!SetFilePointerEx(m_handle, li, &new_pos, method)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }
  return new_pos.QuadPart;
}

// ─── tell ───

nx::expected<int64_t>
FileStream::tell() const
{
  if (!is_open()) {
    return std::unexpected(make_error_code(FileErrc::kNotOpen));
  }

  // 오프셋 0으로 FILE_CURRENT 기준 이동 → 현재 위치 반환
  LARGE_INTEGER zero{};
  LARGE_INTEGER current_pos{};

  if (!SetFilePointerEx(m_handle, zero, &current_pos, FILE_CURRENT)) {
    return std::unexpected(make_error_code(FileErrc::kSeekFailed));
  }
  return current_pos.QuadPart;
}

// ─── flush ───

std::error_code
FileStream::flush()
{
  if (!is_open()) {
    return make_error_code(FileErrc::kNotOpen);
  }

  if (!FlushFileBuffers(m_handle)) {
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

  LARGE_INTEGER size{};
  if (!GetFileSizeEx(m_handle, &size)) {
    return std::unexpected(make_error_code(FileErrc::kReadFailed));
  }
  return size.QuadPart;
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

#endif // _WIN32
