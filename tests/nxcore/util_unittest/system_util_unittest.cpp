// 파일: system_util_unittest.cpp
// 생성일: 2026-01-16
// 설명: system_util 유닛테스트

#include "nxcore/util/system_util.h"
#include <gtest/gtest.h>
#include <cstdlib>

#ifdef _WIN32
#include <windows.h>
#endif

namespace {

// 테스트용 환경변수 설정 및 정리 헬퍼
class EnvVarGuard
{
public:
  explicit EnvVarGuard(const std::string& var_name, const std::string& value)
      : m_var_name(var_name)
  {
#ifdef _WIN32
    _putenv_s(var_name.c_str(), value.c_str());
#else
    setenv(var_name.c_str(), value.c_str(), 1);
#endif
  }

  ~EnvVarGuard()
  {
#ifdef _WIN32
    _putenv_s(m_var_name.c_str(), "");
#else
    unsetenv(m_var_name.c_str());
#endif
  }

private:
  std::string m_var_name;
};

} // namespace

// ============================================================================
// resolve_env_var 테스트
// ============================================================================

TEST(SystemUtilTest, ResolveEnvVar_ValidEnvVar)
{
  EnvVarGuard guard("TEST_VAR_123", "test_value_456");

  std::string result = nx::util::resolve_env_var("${TEST_VAR_123}");
  EXPECT_EQ(result, "test_value_456");
}

TEST(SystemUtilTest, ResolveEnvVar_NonExistentEnvVar)
{
  std::string result = nx::util::resolve_env_var("${NON_EXISTENT_VAR_XYZ}");
  EXPECT_EQ(result, "${NON_EXISTENT_VAR_XYZ}");
}

TEST(SystemUtilTest, ResolveEnvVar_PlainString)
{
  std::string input = "plain_string_without_env_var";
  std::string result = nx::util::resolve_env_var(input);
  EXPECT_EQ(result, input);
}

TEST(SystemUtilTest, ResolveEnvVar_IncorrectFormat)
{
  // 형식이 맞지 않는 경우
  std::string input1 = "$TEST_VAR";
  std::string result1 = nx::util::resolve_env_var(input1);
  EXPECT_EQ(result1, input1);

  std::string input2 = "{TEST_VAR}";
  std::string result2 = nx::util::resolve_env_var(input2);
  EXPECT_EQ(result2, input2);

  std::string input3 = "${TEST_VAR";
  std::string result3 = nx::util::resolve_env_var(input3);
  EXPECT_EQ(result3, input3);
}

TEST(SystemUtilTest, ResolveEnvVar_EmptyString)
{
  std::string result = nx::util::resolve_env_var("");
  EXPECT_EQ(result, "");
}

TEST(SystemUtilTest, ResolveEnvVar_TooShort)
{
  std::string result = nx::util::resolve_env_var("${");
  EXPECT_EQ(result, "${");
}

TEST(SystemUtilTest, ResolveEnvVar_EmptyVarName)
{
  std::string result = nx::util::resolve_env_var("${}");
  EXPECT_EQ(result, "${}");
}

TEST(SystemUtilTest, ResolveEnvVar_WithSpaces)
{
  EnvVarGuard guard("TEST_VAR_WITH_SPACES", "value with spaces");

  std::string result = nx::util::resolve_env_var("${TEST_VAR_WITH_SPACES}");
  EXPECT_EQ(result, "value with spaces");
}

TEST(SystemUtilTest, ResolveEnvVar_WithSpecialChars)
{
  EnvVarGuard guard("TEST_VAR_SPECIAL", "value@#$%^&*()");

  std::string result = nx::util::resolve_env_var("${TEST_VAR_SPECIAL}");
  EXPECT_EQ(result, "value@#$%^&*()");
}

// ============================================================================
// 파일 핸들 제한 테스트
// ============================================================================

TEST(SystemUtilTest, GetMaxStdioHandles)
{
  auto result = nx::util::get_max_stdio_handles();
  ASSERT_TRUE(result.has_value());
  EXPECT_GT(result.value(), 0);
}

TEST(SystemUtilTest, SetMaxStdioHandles_ValidValue)
{
  // 현재 값 저장
  auto original = nx::util::get_max_stdio_handles();
  ASSERT_TRUE(original.has_value());

  // 2048로 설정
  auto result = nx::util::set_max_stdio_handles(2048);
  ASSERT_TRUE(result.has_value());
  EXPECT_GE(result.value(), 512); // 최소 기본값 이상

  // 확인
  auto current = nx::util::get_max_stdio_handles();
  ASSERT_TRUE(current.has_value());
  EXPECT_GE(current.value(), 512);

  // 원래 값 복원
  auto restored = nx::util::set_max_stdio_handles(original.value());
  (void)restored;
}

TEST(SystemUtilTest, SetMaxStdioHandles_InvalidValue)
{
  auto result = nx::util::set_max_stdio_handles(0);
  EXPECT_FALSE(result.has_value());

  auto result2 = nx::util::set_max_stdio_handles(-1);
  EXPECT_FALSE(result2.has_value());
}