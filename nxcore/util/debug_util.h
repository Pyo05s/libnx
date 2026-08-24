// nxcore/debug_util.h
// 2025-11-24
// 설명: 디버그 유틸리티

#pragma once

#include <stdexcept>
#include <string>

#ifdef _DEBUG

// 디버거 연결 시 호출 지점에서 즉시 중단
#if defined(_MSC_VER)
#define NX_DEBUG_BREAK() __debugbreak()
#elif defined(__GNUC__) || defined(__clang__)
#define NX_DEBUG_BREAK() __builtin_trap()
#else
#include <cassert>
#define NX_DEBUG_BREAK() assert(false)
#endif

#define NX_ASSERT(expr)                                                              \
  do {                                                                               \
    if (!(expr)) {                                                                   \
      NX_DEBUG_BREAK();                                                              \
    }                                                                                \
  } while (0)

// 디버그 모드: debug break 후 예외 throw
// 릴리즈 모드: 예외만 throw
#define NX_ASSERT_THROW(expr, exception_type, message)                               \
  do {                                                                               \
    if (!(expr)) {                                                                   \
      NX_DEBUG_BREAK();                                                              \
      throw exception_type(message);                                                 \
    }                                                                                \
  } while (0)

#else

#define NX_ASSERT(expr) ((void)0)

// 릴리즈 모드: 예외만 throw
#define NX_ASSERT_THROW(expr, exception_type, message)                               \
  do {                                                                               \
    if (!(expr)) {                                                                   \
      throw exception_type(message);                                                 \
    }                                                                                \
  } while (0)

#endif // _DEBUG

// null 포인터 검사: 디버그에서는 debug break + 예외, 릴리즈에서는 예외만
#define NX_REQUIRE_NON_NULL(ptr, name)                                               \
  do {                                                                               \
    if (!(ptr)) {                                                                    \
      const auto error_msg = std::string(name) + " cannot be null";                  \
      NX_ASSERT(false);                                                              \
      throw std::invalid_argument(error_msg);                                        \
    }                                                                                \
  } while (0)