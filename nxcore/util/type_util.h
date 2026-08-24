// nx::type_util.h
// 2025-11-24
// 설명: 공용 타입 유틸리티 정의

#pragma once

#include <chrono>
#include <concepts>
#include <expected>
#include <system_error>
#include <type_traits>

#include <boost/asio/awaitable.hpp>

namespace nx {

#define NX_NODISCARD [[nodiscard]]

// 복사 생성자 및 복사 대입 연산자 삭제 매크로
#define NX_NON_COPYABLE(ClassName)                                                       \
  ClassName(const ClassName&) = delete;                                                  \
  ClassName& operator=(const ClassName&) = delete

// 이동 생성자 및 이동 대입 연산자 삭제 매크로
#define NX_NON_MOVABLE(ClassName)                                                        \
  ClassName(ClassName&&) = delete;                                                       \
  ClassName& operator=(ClassName&&) = delete

// 복사 및 이동 생성자/대입 연산자 모두 삭제 매크로
#define NX_NON_COPYABLE_AND_MOVABLE(ClassName)                                           \
  NX_NON_COPYABLE(ClassName);                                                            \
  NX_NON_MOVABLE(ClassName)

// 인스턴스화 불가능 클래스 매크로 (정적 메서드만 제공하는 유틸리티 클래스용)
#define NX_NON_INSTANTIABLE(ClassName)                                                   \
  ClassName() = delete;                                                                  \
  ~ClassName() = delete;                                                                 \
  NX_NON_COPYABLE_AND_MOVABLE(ClassName)

// 사용되지 않는 변수 표시 매크로
#define NX_UNUSED(x) (void)(x)

#define NX_NODISCARD [[nodiscard]]

// ============================================================================
// std::chrono 단축 별칭
// ============================================================================
using nanoseconds = std::chrono::nanoseconds;
using microseconds = std::chrono::microseconds;
using milliseconds = std::chrono::milliseconds;
using seconds = std::chrono::seconds;
using minutes = std::chrono::minutes;
using hours = std::chrono::hours;

#if _HAS_CXX20
using days = std::chrono::days;
using weeks = std::chrono::weeks;
using years = std::chrono::years;
using months = std::chrono::months;
#endif // _HAS_CXX20

template <typename To, typename Rep, typename Period>
NX_NODISCARD constexpr auto
duration_count(const std::chrono::duration<Rep, Period>& duration)
{
  return std::chrono::duration_cast<To>(duration).count();
}

// ============================================================================
// expected / unexpected 단축 별칭
// ============================================================================
template <typename T, typename E = std::error_code>
using expected = std::expected<T, E>;

template <typename E>
using unexpected = std::unexpected<E>;

/// nx::awaitable_expected<T, E>> 단축 별칭
/// E 기본값: std::error_code
template <typename T, typename E = std::error_code>
using awaitable_expected = boost::asio::awaitable<std::expected<T, E>>;

/// nx::awaitable<T> 단축 별칭
template <typename T>
using awaitable = boost::asio::awaitable<T>;

} // namespace nx
