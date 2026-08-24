// 파일: path_util.h
// 생성일: 2026-03-25
// 설명: 파일 시스템 경로 유효성 검증 유틸리티

#pragma once

#include <string>
#include <string_view>

namespace nx {

/// Windows 경로에 사용할 수 없는 문자가 포함되어 있는지 검사
/// @param path 검사할 경로 문자열
/// @return 유효하면 빈 문자열, 유효하지 않으면 오류 메시지
inline std::string
validate_directory_path(std::string_view path)
{
  if (path.empty()) {
    return "Directory path must not be empty";
  }

  // Windows 파일/폴더명에 사용할 수 없는 문자 (드라이브 ':', 경로 구분자 '\\', '/'
  // 제외)
  constexpr std::string_view kInvalidChars = "<>\"|?*";

  for (char ch : path) {
    if (kInvalidChars.find(ch) != std::string_view::npos) {
      return std::string("Path contains invalid character: '") + ch + "'";
    }
    // 제어 문자 (0x00~0x1F) 차단
    if (static_cast<unsigned char>(ch) < 0x20) {
      return "Path contains control character";
    }
  }

  return {};
}

} // namespace nx
