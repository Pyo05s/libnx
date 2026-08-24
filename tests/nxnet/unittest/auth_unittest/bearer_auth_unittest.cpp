// 파일: bearer_auth_unittest.cpp
// 생성일: 2026-02-10
// 설명: Bearer Token 인증 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/bearer/bearer_auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include <thread>
#include <vector>

using namespace nx::net::auth;

// 기본 Bearer Token 헤더 생성
TEST(BearerAuthTest, GenerateHeader)
{
  auto provider_result = BearerAuthProvider::create("test_token_12345");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"GET", "/api/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer test_token_12345");
}

// OAuth 2.0 액세스 토큰 시나리오
TEST(BearerAuthTest, OAuth2AccessToken)
{
  std::string access_token = "ya29.a0AfH6SMBx...";
  auto provider_result = BearerAuthProvider::create(access_token);
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"GET", "/api/user/profile"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer " + access_token);
}

// JWT 토큰 시나리오 (RFC 7519)
TEST(BearerAuthTest, JwtToken)
{
  std::string jwt_token
    = "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5MDIyf"
      "Q."
      "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";

  auto provider_result = BearerAuthProvider::create(jwt_token);
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"POST", "/api/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer " + jwt_token);
}

// 토큰 갱신 테스트
TEST(BearerAuthTest, TokenUpdate)
{
  auto provider_result = BearerAuthProvider::create("old_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"GET", "/api/resource"};
  auto result1 = provider.generate_authorization_header(context);
  ASSERT_TRUE(result1.has_value());
  EXPECT_EQ(*result1, "Bearer old_token");

  // 토큰 갱신
  auto update_ec = provider.update_token("new_token");
  EXPECT_FALSE(update_ec);

  auto result2 = provider.generate_authorization_header(context);
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(*result2, "Bearer new_token");
  EXPECT_NE(*result1, *result2);
}

// 토큰 조회 테스트
TEST(BearerAuthTest, GetToken)
{
  std::string token = "test_token";
  auto provider_result = BearerAuthProvider::create(token);
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  EXPECT_EQ(provider.get_token(), token);

  // 토큰 갱신 후 조회
  auto update_ec = provider.update_token("updated_token");
  EXPECT_FALSE(update_ec);
  EXPECT_EQ(provider.get_token(), "updated_token");
}

// 빈 토큰 생성 시 오류 반환
TEST(BearerAuthTest, EmptyTokenReturnsError)
{
  auto result = BearerAuthProvider::create("");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidParameter));
}

// 빈 토큰으로 갱신 시 오류 반환
TEST(BearerAuthTest, UpdateWithEmptyTokenReturnsError)
{
  auto provider_result = BearerAuthProvider::create("valid_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  auto ec = provider.update_token("");
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, make_error_code(AuthError::kInvalidParameter));

  // 오류 발생 후에도 기존 토큰 유지
  EXPECT_EQ(provider.get_token(), "valid_token");
}

// 캐싱 테스트 (동일한 헤더가 재사용되는지 확인)
TEST(BearerAuthTest, HeaderCaching)
{
  auto provider_result = BearerAuthProvider::create("cached_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context1{"GET", "/path1"};
  auto result1 = provider.generate_authorization_header(context1);

  AuthContext context2{"POST", "/path2"};
  auto result2 = provider.generate_authorization_header(context2);

  // 컨텍스트가 달라도 동일한 헤더 반환
  ASSERT_TRUE(result1.has_value());
  ASSERT_TRUE(result2.has_value());
  EXPECT_EQ(*result1, *result2);
  EXPECT_EQ(*result1, "Bearer cached_token");
}

// 스킴 확인
TEST(BearerAuthTest, SchemeIdentification)
{
  auto provider_result = BearerAuthProvider::create("token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();
  EXPECT_EQ(provider.scheme(), AuthScheme::kBearer);
}

// 복제 테스트
TEST(BearerAuthTest, Clone)
{
  auto provider_result = BearerAuthProvider::create("original_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& original = *provider_result.value();
  auto cloned = original.clone();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->scheme(), AuthScheme::kBearer);

  // 복제된 객체로 헤더 생성
  AuthContext context{"GET", "/api"};
  auto result = cloned->generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer original_token");
}

// 복제 후 독립성 확인
TEST(BearerAuthTest, CloneIndependence)
{
  auto provider_result = BearerAuthProvider::create("original_token");
  ASSERT_TRUE(provider_result.has_value());
  auto original = std::move(provider_result.value());
  auto cloned = original->clone();

  // 원본 삭제
  original.reset();

  // 복제본은 독립적으로 동작
  AuthContext context{"GET", "/test"};
  auto result = cloned->generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer original_token");
}

// 멀티스레드 안전성 테스트: 토큰 갱신
TEST(BearerAuthTest, ConcurrentTokenUpdate)
{
  auto provider_result = BearerAuthProvider::create("initial_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();
  const int num_threads = 10;
  const int updates_per_thread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, i, updates_per_thread]() {
      for (int j = 0; j < updates_per_thread; ++j) {
        std::string new_token
          = "token_" + std::to_string(i) + "_" + std::to_string(j);
        (void)provider.update_token(new_token);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 최종 토큰이 유효한지 확인
  std::string final_token = provider.get_token();
  EXPECT_FALSE(final_token.empty());
  EXPECT_TRUE(final_token.starts_with("token_"));
}

// 멀티스레드 안전성 테스트: 헤더 생성
TEST(BearerAuthTest, ConcurrentHeaderGeneration)
{
  auto provider_result = BearerAuthProvider::create("concurrent_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();
  const int num_threads = 20;
  const int requests_per_thread = 100;

  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, &success_count, requests_per_thread]() {
      AuthContext context{"GET", "/api/resource"};

      for (int j = 0; j < requests_per_thread; ++j) {
        auto result = provider.generate_authorization_header(context);
        if (result.has_value() && *result == "Bearer concurrent_token") {
          ++success_count;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 모든 요청이 성공해야 함
  EXPECT_EQ(success_count, num_threads * requests_per_thread);
}

// 멀티스레드 안전성 테스트: 혼합 작업 (읽기/쓰기/갱신)
TEST(BearerAuthTest, ConcurrentMixedOperations)
{
  auto provider_result = BearerAuthProvider::create("mixed_token");
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();
  const int num_threads = 15;
  const int ops_per_thread = 50;

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, i, ops_per_thread]() {
      AuthContext context{"GET", "/api"};

      for (int j = 0; j < ops_per_thread; ++j) {
        switch (j % 3) {
          case 0:
            // 헤더 생성
            (void)provider.generate_authorization_header(context);
            break;
          case 1:
            // 토큰 조회
            (void)provider.get_token();
            break;
          case 2:
            // 토큰 갱신
            (void)provider.update_token(
              "token_" + std::to_string(i) + "_" + std::to_string(j));
            break;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 최종 상태가 일관성 있는지 확인
  std::string final_token = provider.get_token();
  AuthContext context{"GET", "/test"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer " + final_token);
}

// 팩토리 메서드 테스트
TEST(BearerAuthTest, FactoryCreate)
{
  auto provider = AuthProviderFactory::create_bearer("factory_token");

  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->scheme(), AuthScheme::kBearer);

  AuthContext context{"GET", "/api/resource"};
  auto result = provider->generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer factory_token");
}

// 긴 토큰 처리
TEST(BearerAuthTest, LongToken)
{
  std::string long_token(5000, 'x');
  auto provider_result = BearerAuthProvider::create(long_token);
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"GET", "/"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer " + long_token);
}

// 특수 문자가 포함된 토큰
TEST(BearerAuthTest, TokenWithSpecialCharacters)
{
  std::string token = "token-with_special.chars/and+symbols=";
  auto provider_result = BearerAuthProvider::create(token);
  ASSERT_TRUE(provider_result.has_value());
  auto& provider = *provider_result.value();

  AuthContext context{"GET", "/api"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(*result, "Bearer " + token);
}
