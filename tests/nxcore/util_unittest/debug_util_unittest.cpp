// 파일: debug_util_unittest.cpp
// 생성일: 2025-12-03
// 설명: debug_util 유닛테스트

#include "nxcore/util/debug_util.h"
#include <gtest/gtest.h>
#include <memory>

#ifdef NX_ASSERT
#undef NX_ASSERT
#define NX_ASSERT(expr) ((void)0) // 테스트 중에는 assert 무시
#endif

#ifdef NX_DEBUG_BREAK
#undef NX_DEBUG_BREAK
#define NX_DEBUG_BREAK() ((void)0) // 테스트 중에는 debug break 무시
#endif

namespace {

class DummyClass
{
public:
  explicit DummyClass(std::shared_ptr<int> ptr)
      : m_ptr(std::move(ptr))
  {
    NX_REQUIRE_NON_NULL(m_ptr, "DummyClass::ptr");
  }

private:
  std::shared_ptr<int> m_ptr;
};

} // namespace

// NX_REQUIRE_NON_NULL 테스트: 정상 케이스
TEST(DebugUtilTest, RequireNonNull_ValidPointer)
{
  auto ptr = std::make_shared<int>(42);
  EXPECT_NO_THROW(DummyClass obj(ptr));
}

// NX_REQUIRE_NON_NULL 테스트: null 포인터는 예외 발생
TEST(DebugUtilTest, RequireNonNull_NullPointer)
{
  std::shared_ptr<int> null_ptr;

  EXPECT_THROW({ DummyClass obj(null_ptr); }, std::invalid_argument);
}

// NX_ASSERT_THROW 테스트: 조건 만족
TEST(DebugUtilTest, AssertThrow_ConditionTrue)
{
  int value = 10;

  EXPECT_NO_THROW(
    { NX_ASSERT_THROW(value > 0, std::logic_error, "Value must be positive"); });
}

// NX_ASSERT_THROW 테스트: 조건 불만족
TEST(DebugUtilTest, AssertThrow_ConditionFalse)
{
  int value = -1;

  EXPECT_THROW(
    { NX_ASSERT_THROW(value > 0, std::logic_error, "Value must be positive"); },
    std::logic_error);
}

// NX_ASSERT_THROW 테스트: 예외 메시지 확인
TEST(DebugUtilTest, AssertThrow_ExceptionMessage)
{
  int value = -1;

  try {
    NX_ASSERT_THROW(value > 0, std::runtime_error, "Expected positive value");
    FAIL() << "예외가 발생해야 합니다";
  }
  catch (const std::runtime_error& e) {
    EXPECT_STREQ(e.what(), "Expected positive value");
  }
}

// NX_REQUIRE_NON_NULL 테스트: 예외 메시지 확인
TEST(DebugUtilTest, RequireNonNull_ExceptionMessage)
{
  std::shared_ptr<int> null_ptr;

  try {
    NX_REQUIRE_NON_NULL(null_ptr, "TestPointer");
    FAIL() << "예외가 발생해야 합니다";
  }
  catch (const std::invalid_argument& e) {
    std::string message = e.what();
    EXPECT_NE(message.find("TestPointer"), std::string::npos);
    EXPECT_NE(message.find("cannot be null"), std::string::npos);
  }
}
