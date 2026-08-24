// 파일: enum_util.h
// 생성일: 2025-12-09
// 설명: 열거형(enum) 관련 유틸리티 함수들

#pragma once

#include <type_traits>
#include <concepts>
#include <functional>

namespace nx {

// ===================================================================
// enum class 해시 함수 (unordered_map/unordered_set 키 용도)
// ===================================================================

/// enum class를 unordered 컨테이너 키로 사용하기 위한 해시 펑터
struct EnumHash
{
  template <typename E>
  requires std::is_enum_v<E>
  std::size_t operator()(E value) const noexcept
  {
    return std::hash<std::underlying_type_t<E>>{}(
      static_cast<std::underlying_type_t<E>>(value));
  }
};

// ===================================================================
// 열거형 비교 함수들
// ===================================================================

// 열거형과 그 기본 타입 간의 비교 함수
template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_equal(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return lhs == static_cast<std::underlying_type_t<E>>(rhs);
}

template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_not_equal(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return !cmp_enum_equal(lhs, rhs);
}

template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_less(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return lhs < static_cast<std::underlying_type_t<E>>(rhs);
}

template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_greater(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return lhs > static_cast<std::underlying_type_t<E>>(rhs);
}

template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_less_equal(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return lhs <= static_cast<std::underlying_type_t<E>>(rhs);
}

template <typename E>
requires std::is_enum_v<E>
bool
cmp_enum_greater_equal(std::underlying_type_t<E> lhs, E rhs) noexcept
{
  return lhs >= static_cast<std::underlying_type_t<E>>(rhs);
}

// ===================================================================
// 비트 연산 비교 함수들
// enum 플래그 검사 (예: BlockFlags, 비트마스킹)
// ===================================================================

// 모든 플래그가 설정되어 있는지 확인
// 예: has_all_flags(value, BlockFlags::kCompressed | BlockFlags::kHasKeyFrame)
template <typename E>
requires std::is_enum_v<E>
bool
has_all_flags(std::underlying_type_t<E> value, E flags) noexcept
{
  auto flags_val = static_cast<std::underlying_type_t<E>>(flags);
  return (value & flags_val) == flags_val;
}

// 어떤 플래그라도 설정되어 있는지 확인
// 예: has_any_flag(value, BlockFlags::kCompressed | BlockFlags::kHasKeyFrame)
template <typename E>
requires std::is_enum_v<E>
bool
has_any_flag(std::underlying_type_t<E> value, E flags) noexcept
{
  auto flags_val = static_cast<std::underlying_type_t<E>>(flags);
  return (value & flags_val) != 0;
}

// 특정 플래그가 설정되어 있지 않은지 확인
// 예: !has_flag(value, BlockFlags::kCompressed)
template <typename E>
requires std::is_enum_v<E>
bool
has_flag(std::underlying_type_t<E> value, E flag) noexcept
{
  auto flag_val = static_cast<std::underlying_type_t<E>>(flag);
  return (value & flag_val) == flag_val;
}

// ===================================================================
// 비트 플래그 enum 조작 함수들
// ===================================================================
// 명시적 함수명을 사용하여 enum 플래그 값을 조작
// 사용 예:
//   RecordAttribute attr = RecordAttribute::kNone;
//   attr = set_enum_bit_or(attr, RecordAttribute::kContinuous);
//   attr = set_enum_bit_and(attr, RecordAttribute::kMotion);

// 비트 OR 연산 (플래그 설정)
// 예: set_enum_bit_or(attr, RecordAttribute::kContinuous | RecordAttribute::kMotion)
template <typename E>
requires std::is_enum_v<E>
constexpr E
set_enum_bit_or(E value, E flags) noexcept
{
  return static_cast<E>(
    static_cast<std::underlying_type_t<E>>(value)
    | static_cast<std::underlying_type_t<E>>(flags));
}

// 비트 AND 연산 (플래그 마스킹)
// 예: set_enum_bit_and(attr, RecordAttribute::kContinuous)
template <typename E>
requires std::is_enum_v<E>
constexpr E
set_enum_bit_and(E value, E flags) noexcept
{
  return static_cast<E>(
    static_cast<std::underlying_type_t<E>>(value)
    & static_cast<std::underlying_type_t<E>>(flags));
}

// 비트 XOR 연산 (플래그 토글)
// 예: set_enum_bit_xor(attr, RecordAttribute::kManual)
template <typename E>
requires std::is_enum_v<E>
constexpr E
set_enum_bit_xor(E value, E flags) noexcept
{
  return static_cast<E>(
    static_cast<std::underlying_type_t<E>>(value)
    ^ static_cast<std::underlying_type_t<E>>(flags));
}

// 비트 NOT 연산 (플래그 반전)
// 예: set_enum_bit_not(attr)
template <typename E>
requires std::is_enum_v<E>
constexpr E
set_enum_bit_not(E value) noexcept
{
  return static_cast<E>(~static_cast<std::underlying_type_t<E>>(value));
}

// 플래그 설정 (OR)
// 예: add_enum_flags(attr, RecordAttribute::kContinuous)
template <typename E>
requires std::is_enum_v<E>
constexpr E
add_enum_flags(E value, E flags) noexcept
{
  return set_enum_bit_or(value, flags);
}

// 플래그 제거 (AND NOT)
// 예: remove_enum_flags(attr, RecordAttribute::kManual)
template <typename E>
requires std::is_enum_v<E>
constexpr E
remove_enum_flags(E value, E flags) noexcept
{
  return set_enum_bit_and(value, set_enum_bit_not(flags));
}

// 플래그 토글
// 예: toggle_enum_flags(attr, RecordAttribute::kMotion)
template <typename E>
requires std::is_enum_v<E>
constexpr E
toggle_enum_flags(E value, E flags) noexcept
{
  return set_enum_bit_xor(value, flags);
}

// 플래그가 정확히 일치하는지 확인 (모든 비트가 같음)
// 예: enum_flags_equal(attr, RecordAttribute::kContinuous)
template <typename E>
requires std::is_enum_v<E>
constexpr bool
enum_flags_equal(E value, E flags) noexcept
{
  return static_cast<std::underlying_type_t<E>>(value)
         == static_cast<std::underlying_type_t<E>>(flags);
}

} // namespace nx
