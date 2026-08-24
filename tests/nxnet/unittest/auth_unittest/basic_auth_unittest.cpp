// 파일: basic_auth_unittest.cpp
// 생성일: 2026-02-10
// 설명: Basic 인증 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/basic/basic_auth_provider.h"
#include "nxnet/auth/auth_types.h"

using namespace nx::net::auth;

// RFC 7617 예제: "Aladdin:open sesame" -> "QWxhZGRpbjpvcGVuIHNlc2FtZQ=="
TEST(BasicAuthTest, Rfc7617Example)
{
  Credentials creds{"Aladdin", "open sesame"};
  BasicAuthProvider provider(creds);

  AuthContext context{"GET", "/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==");
}

// 일반적인 자격 증명 테스트
TEST(BasicAuthTest, GenerateHeader)
{
  Credentials creds{"admin", "password"};
  BasicAuthProvider provider(creds);

  AuthContext context{"GET", "/api/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic YWRtaW46cGFzc3dvcmQ=");
}

// 복잡한 비밀번호 테스트
TEST(BasicAuthTest, ComplexPassword)
{
  Credentials creds{"user", "p@ssw0rd!#$"};
  BasicAuthProvider provider(creds);

  AuthContext context{"POST", "/login"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic dXNlcjpwQHNzdzByZCEjJA==");
}

// 특수 문자가 포함된 사용자명
TEST(BasicAuthTest, SpecialCharactersInUsername)
{
  Credentials creds{"user@domain.com", "pass"};
  BasicAuthProvider provider(creds);

  AuthContext context{"GET", "/"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic dXNlckBkb21haW4uY29tOnBhc3M=");
}

// 빈 비밀번호 테스트
TEST(BasicAuthTest, EmptyPassword)
{
  Credentials creds{"user", ""};
  BasicAuthProvider provider(creds);

  AuthContext context{"GET", "/"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic dXNlcjo=");
}

// 캐싱 테스트 (동일한 헤더가 재사용되는지 확인)
TEST(BasicAuthTest, HeaderCaching)
{
  Credentials creds{"test", "test123"};
  BasicAuthProvider provider(creds);

  AuthContext context1{"GET", "/path1"};
  auto result1 = provider.generate_authorization_header(context1);

  AuthContext context2{"POST", "/path2"};
  auto result2 = provider.generate_authorization_header(context2);

  // 컨텍스트가 달라도 동일한 헤더 반환
  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(*result1, *result2);
}

// 스킴 확인
TEST(BasicAuthTest, SchemeIdentification)
{
  Credentials creds{"user", "pass"};
  BasicAuthProvider provider(creds);

  EXPECT_EQ(provider.scheme(), AuthScheme::kBasic);
}

// Clone 테스트
TEST(BasicAuthTest, CloneProvider)
{
  Credentials creds{"original", "password"};
  BasicAuthProvider provider(creds);

  auto cloned = provider.clone();
  ASSERT_NE(cloned, nullptr);

  AuthContext context{"GET", "/"};
  auto result1 = provider.generate_authorization_header(context);
  auto result2 = cloned->generate_authorization_header(context);

  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(*result1, *result2);
}

// 팩토리를 통한 생성 테스트
TEST(BasicAuthTest, FactoryCreation)
{
  Credentials creds{"factory", "test"};
  auto provider = AuthProviderFactory::create_basic(creds);

  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->scheme(), AuthScheme::kBasic);

  AuthContext context{"GET", "/"};
  auto result = provider->generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Basic ZmFjdG9yeTp0ZXN0");
}

// UTF-8 문자 테스트
TEST(BasicAuthTest, Utf8Characters)
{
  Credentials creds{"사용자", "비밀번호"};
  BasicAuthProvider provider(creds);

  AuthContext context{"GET", "/"};
  auto result = provider.generate_authorization_header(context);

  // UTF-8 인코딩이 올바르게 처리되는지 확인
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->empty());
  EXPECT_TRUE(result->starts_with("Basic "));
}