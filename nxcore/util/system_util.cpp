// 파일: system_util.cpp
// 생성일: 2026-01-16
// 설명: 시스템 관련 유틸리티 함수 구현

#include "nxcore/util/system_util.h"
#include <cstdlib>

#ifdef _WIN32
#include <cstring>
#include <cstdio> // _setmaxstdio, _getmaxstdio
#else
#include <sys/resource.h>
#endif

namespace nx {
namespace util {

std::string
resolve_env_var(const std::string& value)
{
  // ${VAR_NAME} 형식이 아니면 원본 값 그대로 반환
  if (value.size() < 4 || !value.starts_with("${") || !value.ends_with("}")) {
    return value;
  }

  // 환경변수 이름 추출
  std::string var_name = value.substr(2, value.size() - 3);

  // 환경변수 값 조회
#ifdef _WIN32
  char* env_value = nullptr;
  size_t len = 0;
  errno_t err = _dupenv_s(&env_value, &len, var_name.c_str());
  if (err != 0 || env_value == nullptr) {
    return value; // 환경변수가 없으면 원본 값 반환
  }
  std::string result(env_value);
  free(env_value);
  return result;
#else
  const char* env_value = std::getenv(var_name.c_str());
  if (env_value == nullptr) {
    return value; // 환경변수가 없으면 원본 값 반환
  }
  return std::string(env_value);
#endif
}

std::string
resolve_env_vars(const std::string& value)
{
  std::string result;
  result.reserve(value.size());

  std::size_t pos = 0;
  while (pos < value.size()) {
    // ${...} 패턴 시작 위치 탐색
    auto start = value.find("${", pos);
    if (start == std::string::npos) {
      result.append(value, pos);
      break;
    }

    // ${ 이전 부분 복사
    result.append(value, pos, start - pos);

    // } 닫힘 위치 탐색
    auto end = value.find('}', start + 2);
    if (end == std::string::npos) {
      // 닫히지 않은 패턴은 원본 그대로 유지
      result.append(value, start);
      break;
    }

    // ${VAR_NAME} 토큰을 resolve_env_var()로 치환
    auto token = value.substr(start, end - start + 1);
    result.append(resolve_env_var(token));

    pos = end + 1;
  }

  return result;
}

nx::expected<int>
get_max_stdio_handles()
{
#ifdef _WIN32
  int current = _getmaxstdio();
  return current;
#else
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
    return std::unexpected(std::error_code(errno, std::system_category()));
  }
  return static_cast<int>(rl.rlim_cur);
#endif
}

nx::expected<int>
set_max_stdio_handles(int count)
{
  if (count <= 0) {
    return std::unexpected(std::make_error_code(std::errc::invalid_argument));
  }

#ifdef _WIN32
  // Windows CRT: _setmaxstdio 범위 [512, 8192]
  int result = _setmaxstdio(count);
  if (result == -1) {
    return std::unexpected(std::error_code(errno, std::system_category()));
  }
  return result;
#else
  struct rlimit rl;
  if (getrlimit(RLIMIT_NOFILE, &rl) != 0) {
    return std::unexpected(std::error_code(errno, std::system_category()));
  }

  rl.rlim_cur = static_cast<rlim_t>(count);
  if (rl.rlim_cur > rl.rlim_max) {
    rl.rlim_cur = rl.rlim_max;
  }

  if (setrlimit(RLIMIT_NOFILE, &rl) != 0) {
    return std::unexpected(std::error_code(errno, std::system_category()));
  }

  return static_cast<int>(rl.rlim_cur);
#endif
}

} // namespace util
} // namespace nx