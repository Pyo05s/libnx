// 파일: platform_util.cpp
// 생성일: 2026-04-28
// 설명: 플랫폼 독립적인 프로세스 정보 조회 및 워크스페이스 디렉터리 관리 구현

#include "nxcore/util/platform_util.h"

#include <mutex>
#include <optional>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#elif defined(__APPLE__)
#include <mach-o/dyld.h>
#include <vector>
#else
#include <unistd.h>
#endif

namespace nx {
namespace util {

namespace {

// ============================================================================
// 전역 워크스페이스 상태
// ============================================================================

std::mutex g_workspace_mutex;
std::optional<std::filesystem::path> g_workspace_dir;

} // anonymous namespace

// ============================================================================
// 프로세스 정보
// ============================================================================

nx::expected<std::filesystem::path>
get_process_path()
{
#ifdef _WIN32
  // Windows: GetModuleFileNameW로 실행 파일 경로 획득
  wchar_t buffer[MAX_PATH + 1]{};
  const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
  if (length == 0) {
    return std::unexpected(
      std::error_code(static_cast<int>(::GetLastError()), std::system_category()));
  }
  return std::filesystem::path(buffer);

#elif defined(__APPLE__)
  // macOS: _NSGetExecutablePath로 실행 파일 경로 획득
  uint32_t size = 0;
  _NSGetExecutablePath(nullptr, &size); // 필요한 버퍼 크기 조회

  std::vector<char> buffer(size);
  if (_NSGetExecutablePath(buffer.data(), &size) != 0) {
    return std::unexpected(std::make_error_code(std::errc::no_such_file_or_directory));
  }

  std::error_code ec;
  auto resolved = std::filesystem::canonical(buffer.data(), ec);
  if (ec) {
    return std::unexpected(ec);
  }
  return resolved;

#else
  // Linux: /proc/self/exe 심볼릭 링크 해석
  std::error_code ec;
  auto resolved = std::filesystem::read_symlink("/proc/self/exe", ec);
  if (ec) {
    return std::unexpected(ec);
  }
  return resolved;
#endif
}

nx::expected<std::string>
get_process_name()
{
  auto path_result = get_process_path();
  if (!path_result) {
    return std::unexpected(path_result.error());
  }
  return path_result->filename().string();
}

// ============================================================================
// 워크스페이스 디렉터리
// ============================================================================

std::filesystem::path
get_workspace_directory()
{
  std::lock_guard lock(g_workspace_mutex);

  if (g_workspace_dir.has_value()) {
    return *g_workspace_dir;
  }

  // 기본값: 실행 파일이 위치한 디렉터리
  // mutex를 잠근 상태에서 get_process_path()를 호출하면 deadlock 위험이 없음
  // (get_process_path는 g_workspace_mutex를 사용하지 않음)
  auto path_result = get_process_path();
  if (path_result) {
    return path_result->parent_path();
  }

  // 실행 파일 경로 조회 실패 시 OS CWD를 반환
  std::error_code ec;
  auto cwd = std::filesystem::current_path(ec);
  if (ec) {
    return {};
  }
  return cwd;
}

std::error_code
set_workspace_directory(const std::filesystem::path& path)
{
  // 경로 존재 여부 검증
  // is_directory() 실패 시 OS별 error_category가 달라지므로
  // 항상 표준 generic 카테고리로 통일하여 반환
  std::error_code ec;
  if (!std::filesystem::is_directory(path, ec)) {
    return std::make_error_code(std::errc::no_such_file_or_directory);
  }

  // 정규화된 절대 경로로 변환
  auto canonical_path = std::filesystem::weakly_canonical(path, ec);
  if (ec) {
    return ec;
  }

  // OS CWD 변경
  std::filesystem::current_path(canonical_path, ec);
  if (ec) {
    return ec;
  }

  // 내부 상태 업데이트
  std::lock_guard lock(g_workspace_mutex);
  g_workspace_dir = canonical_path;

  return {};
}

} // namespace util
} // namespace nx
