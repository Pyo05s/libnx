// 파일: coroutine_helper.h
// 생성일: 2026-01-02
// 설명: 코루틴 내에서 사용 가능한 GTest 매크로 정의

#pragma once

#include <gtest/gtest.h>

// ============================================================================
// 코루틴용 ASSERT 매크로
// ============================================================================
// ASSERT는 실패 시 GTest에 실패를 기록하고 즉시 코루틴을 종료합니다 (co_return 사용)
// EXPECT를 먼저 호출하여 실패를 기록하고, 실패 시 co_return으로 종료합니다.

#define CO_ASSERT_TRUE(condition)                                                    \
  do {                                                                               \
    EXPECT_TRUE(condition);                                                          \
    if (!(condition)) {                                                              \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_FALSE(condition)                                                   \
  do {                                                                               \
    EXPECT_FALSE(condition);                                                         \
    if (condition) {                                                                 \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_EQ(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_EQ(val1, val2);                                                           \
    if (!((val1) == (val2))) {                                                       \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_NE(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_NE(val1, val2);                                                           \
    if (!((val1) != (val2))) {                                                       \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_LT(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_LT(val1, val2);                                                           \
    if (!((val1) < (val2))) {                                                        \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_LE(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_LE(val1, val2);                                                           \
    if (!((val1) <= (val2))) {                                                       \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_GT(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_GT(val1, val2);                                                           \
    if (!((val1) > (val2))) {                                                        \
      co_return;                                                                     \
    }                                                                                \
  } while (0)

#define CO_ASSERT_GE(val1, val2)                                                     \
  do {                                                                               \
    EXPECT_GE(val1, val2);                                                           \
    if (!((val1) >= (val2))) {                                                       \
      co_return;                                                                     \
    }                                                                                \
  } while (0)
