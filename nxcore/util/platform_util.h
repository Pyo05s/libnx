// 파일: platform_util.h
// 생성일: 2026-04-28
// 설명: 플랫폼 독립적인 프로세스 정보 조회 및 워크스페이스 디렉터리 관리 유틸리티

#pragma once

#include <nxcore/util/type_util.h>

#include <filesystem>
#include <string>
#include <expected>
#include <system_error>

namespace nx {
namespace util {

// ============================================================================
// 프로세스 정보
// ============================================================================

/// 현재 프로세스 실행 파일의 전체 경로를 반환
/// - Windows: GetModuleFileNameW
/// - Linux:   /proc/self/exe (readlink)
/// - macOS:   _NSGetExecutablePath
///
/// @return 성공 시 실행 파일 절대 경로, 실패 시 에러 코드
nx::expected<std::filesystem::path> get_process_path();

/// 현재 프로세스 실행 파일 이름을 반환 (확장자 포함)
/// - 예: Windows에서 "record_server.exe", Linux에서 "record_server"
///
/// @return 성공 시 파일 이름 문자열, 실패 시 에러 코드
nx::expected<std::string> get_process_name();

// ============================================================================
// 워크스페이스 디렉터리
// ============================================================================

/// 현재 워크스페이스 디렉터리를 반환
/// - set_workspace_directory() 호출 이전: 실행 파일 위치의 디렉터리를 기본값으로
/// 반환
/// - set_workspace_directory() 호출 이후: 설정된 경로를 반환
/// - 실행 파일 경로 조회 실패 시: OS 현재 작업 디렉터리(CWD)를 반환
///
/// @note 스레드 안전
std::filesystem::path get_workspace_directory();

/// 워크스페이스 디렉터리를 설정하고 OS 현재 작업 디렉터리(CWD)도 함께 변경
/// - 상대 경로 해석 시 기준 디렉터리로 활용
/// - std::filesystem::current_path()도 동일 경로로 변경
///
/// @param path 설정할 디렉터리 경로 (반드시 존재해야 함)
/// @return 성공 시 std::error_code{}, 실패 시 에러 코드
///         - 경로가 존재하지 않거나 디렉터리가 아닌 경우:
///         std::errc::no_such_file_or_directory
/// @note 스레드 안전
std::error_code set_workspace_directory(const std::filesystem::path& path);

} // namespace util
} // namespace nx
