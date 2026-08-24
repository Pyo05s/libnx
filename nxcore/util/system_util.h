// 파일: system_util.h
// 생성일: 2026-01-16
// 설명: 시스템 관련 유틸리티 함수

#pragma once

#include <nxcore/util/type_util.h>

#include <string>
#include <expected>
#include <system_error>

namespace nx {
namespace util {

// 환경변수를 해석하여 실제 값을 반환
// - ${VAR_NAME} 형식의 문자열이 주어지면 환경변수 값으로 치환
// - 환경변수가 존재하지 않으면 원본 문자열 그대로 반환
// - ${...} 형식이 아니면 원본 문자열 그대로 반환
std::string resolve_env_var(const std::string& value);

// 문자열 내 모든 ${VAR_NAME} 패턴을 환경변수 값으로 치환
// - resolve_env_var()를 반복 적용하여 복수 패턴 치환 지원
// - 예: "host=${DB_HOST}:${DB_PORT}" → "host=localhost:3306"
// - 환경변수가 없으면 빈 문자열로 치환
std::string resolve_env_vars(const std::string& value);

// CRT 파일 핸들 최대 수 조회
// Windows: _getmaxstdio() 반환
// Linux/macOS: getrlimit(RLIMIT_NOFILE) soft limit 반환
nx::expected<int> get_max_stdio_handles();

// CRT 파일 핸들 최대 수 설정
// Windows: _setmaxstdio(count). 기본 512, 최대 8192.
// Linux/macOS: setrlimit(RLIMIT_NOFILE) soft limit 설정
// @param count 설정할 최대 핸들 수
// @return 성공 시 실제 설정된 값, 실패 시 에러 코드
nx::expected<int> set_max_stdio_handles(int count);

} // namespace util
} // namespace nx