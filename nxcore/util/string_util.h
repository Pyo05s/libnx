// 파일: string_util.h
// 생성일: 2026-04-01
// 설명: 문자열 유틸리티 함수

#pragma once

#include <string_view>

namespace nx {

/// ASCII 소문자 변환 (constexpr)
constexpr char
to_lower(char c)
{
  return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
}

/// ASCII 대소문자 무시 비교 (constexpr)
constexpr bool
iequals(std::string_view a, std::string_view b)
{
  if (a.size() != b.size()) {
    return false;
  }
  for (std::size_t i = 0; i < a.size(); ++i) {
    if (to_lower(a[i]) != to_lower(b[i])) {
      return false;
    }
  }
  return true;
}

} // namespace nx
