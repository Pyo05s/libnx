// 파일: apikey_auth_unittest.cpp
// 생성일: 2026-02-10
// 설명: API Key 인증 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/apikey/apikey_auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include <thread>
#include <vector>

using namespace nx::net::auth;

// 기본 커스텀 헤더 생성
TEST(ApiKeyAuthTest, CustomHeaderGeneration)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("abc123", "X-API-Key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/api/resource"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: abc123");
}

// Authorization 헤더 생성
TEST(ApiKeyAuthTest, AuthHeaderGeneration)
{
  auto result = ApiKeyAuthProvider::create_with_auth_header("xyz789", "ApiKey");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/api/resource"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "ApiKey xyz789");
}

// 기본 헤더 이름 사용
TEST(ApiKeyAuthTest, DefaultHeaderName)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("key123");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_EQ(provider.get_header_name(), "X-API-Key");
  EXPECT_FALSE(provider.uses_auth_header());
}

// 커스텀 헤더 이름
TEST(ApiKeyAuthTest, CustomHeaderName)
{
  auto result
    = ApiKeyAuthProvider::create_with_custom_header("mykey", "X-Custom-Key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"POST", "/api/data"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-Custom-Key: mykey");
  EXPECT_EQ(provider.get_header_name(), "X-Custom-Key");
}

// 커스텀 Authorization 스킴
TEST(ApiKeyAuthTest, CustomAuthScheme)
{
  auto result = ApiKeyAuthProvider::create_with_auth_header("token456", "Bearer");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/api"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "Bearer token456");
  EXPECT_TRUE(provider.uses_auth_header());
}

// 빈 API Key - 커스텀 헤더
TEST(ApiKeyAuthTest, EmptyKeyCustomHeaderReturnsError)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("", "X-API-Key");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidParameter));
}

// 빈 API Key - Authorization 헤더
TEST(ApiKeyAuthTest, EmptyKeyAuthHeaderReturnsError)
{
  auto result = ApiKeyAuthProvider::create_with_auth_header("", "ApiKey");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidParameter));
}

// 빈 헤더 이름
TEST(ApiKeyAuthTest, EmptyHeaderNameReturnsError)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("key123", "");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidParameter));
}

// 빈 스킴 이름
TEST(ApiKeyAuthTest, EmptySchemeNameReturnsError)
{
  auto result = ApiKeyAuthProvider::create_with_auth_header("key123", "");
  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidParameter));
}

// API Key 갱신
TEST(ApiKeyAuthTest, UpdateKey)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("old_key", "X-API-Key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/api"};
  auto header1 = provider.generate_authorization_header(context);
  ASSERT_TRUE(header1.has_value());
  EXPECT_EQ(*header1, "X-API-Key: old_key");

  // Key 갱신
  auto ec = provider.update_key("new_key");
  EXPECT_FALSE(ec);

  auto header2 = provider.generate_authorization_header(context);
  ASSERT_TRUE(header2.has_value());
  EXPECT_EQ(*header2, "X-API-Key: new_key");
}

// 빈 Key로 갱신 시 오류
TEST(ApiKeyAuthTest, UpdateWithEmptyKeyReturnsError)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("valid_key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  auto ec = provider.update_key("");
  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, make_error_code(AuthError::kInvalidParameter));

  // 기존 키 유지 확인
  EXPECT_EQ(provider.get_key(), "valid_key");
}

// Key 조회
TEST(ApiKeyAuthTest, GetKey)
{
  std::string key = "my_api_key";
  auto result = ApiKeyAuthProvider::create_with_custom_header(key);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_EQ(provider.get_key(), key);

  provider.update_key("updated_key");
  EXPECT_EQ(provider.get_key(), "updated_key");
}

// 스킴 확인
TEST(ApiKeyAuthTest, SchemeIdentification)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
  EXPECT_EQ(provider.scheme(), AuthScheme::kApiKey);
}

// 복제 - 커스텀 헤더
TEST(ApiKeyAuthTest, CloneCustomHeader)
{
  auto result
    = ApiKeyAuthProvider::create_with_custom_header("original_key", "X-Custom");
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();
  auto cloned = original.clone();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->scheme(), AuthScheme::kApiKey);

  AuthContext context{"GET", "/api"};
  auto header = cloned->generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-Custom: original_key");
}

// 복제 - Authorization 헤더
TEST(ApiKeyAuthTest, CloneAuthHeader)
{
  auto result = ApiKeyAuthProvider::create_with_auth_header("auth_key", "Token");
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();
  auto cloned = original.clone();

  ASSERT_NE(cloned, nullptr);

  AuthContext context{"POST", "/api"};
  auto header = cloned->generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "Token auth_key");
}

// 복제 후 독립성
TEST(ApiKeyAuthTest, CloneIndependence)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("key");
  ASSERT_TRUE(result.has_value());
  auto original = std::move(result.value());
  auto cloned = original->clone();

  // 원본 삭제
  original.reset();

  // 복제본은 독립적으로 동작
  AuthContext context{"GET", "/test"};
  auto header = cloned->generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: key");
}

// 팩토리 메서드 테스트
TEST(ApiKeyAuthTest, FactoryCreate)
{
  auto provider = AuthProviderFactory::create_api_key("factory_key", "X-API-Key");

  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->scheme(), AuthScheme::kApiKey);

  AuthContext context{"GET", "/api/resource"};
  auto header = provider->generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: factory_key");
}

// 팩토리 기본 헤더 이름
TEST(ApiKeyAuthTest, FactoryDefaultHeaderName)
{
  auto provider = AuthProviderFactory::create_api_key("test_key");

  ASSERT_NE(provider, nullptr);

  AuthContext context{"GET", "/api"};
  auto header = provider->generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: test_key");
}

// 긴 API Key
TEST(ApiKeyAuthTest, LongApiKey)
{
  std::string long_key(1000, 'x');
  auto result = ApiKeyAuthProvider::create_with_custom_header(long_key);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: " + long_key);
}

// 특수 문자가 포함된 Key
TEST(ApiKeyAuthTest, KeyWithSpecialCharacters)
{
  std::string key = "key-with_special.chars/and+symbols=123";
  auto result = ApiKeyAuthProvider::create_with_custom_header(key, "X-Secret");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"POST", "/api"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-Secret: " + key);
}

// 멀티스레드 안전성: Key 갱신
TEST(ApiKeyAuthTest, ConcurrentKeyUpdate)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("initial_key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
  const int num_threads = 10;
  const int updates_per_thread = 100;

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, i, updates_per_thread]() {
      for (int j = 0; j < updates_per_thread; ++j) {
        std::string new_key = "key_" + std::to_string(i) + "_" + std::to_string(j);
        (void)provider.update_key(new_key);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 최종 키가 유효한지 확인
  std::string final_key = provider.get_key();
  EXPECT_FALSE(final_key.empty());
  EXPECT_TRUE(final_key.starts_with("key_"));
}

// 멀티스레드 안전성: 헤더 생성
TEST(ApiKeyAuthTest, ConcurrentHeaderGeneration)
{
  auto result
    = ApiKeyAuthProvider::create_with_auth_header("concurrent_key", "ApiKey");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
  const int num_threads = 20;
  const int requests_per_thread = 100;

  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, &success_count, requests_per_thread]() {
      AuthContext context{"GET", "/api"};

      for (int j = 0; j < requests_per_thread; ++j) {
        auto header = provider.generate_authorization_header(context);
        if (header.has_value() && *header == "ApiKey concurrent_key") {
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

// 멀티스레드 안전성: 혼합 작업
TEST(ApiKeyAuthTest, ConcurrentMixedOperations)
{
  auto result = ApiKeyAuthProvider::create_with_custom_header("mixed_key");
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
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
            // Key 조회
            (void)provider.get_key();
            break;
          case 2:
            // Key 갱신
            (void)provider.update_key(
              "key_" + std::to_string(i) + "_" + std::to_string(j));
            break;
        }
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 최종 상태가 일관성 있는지 확인
  std::string final_key = provider.get_key();
  AuthContext context{"GET", "/test"};
  auto header = provider.generate_authorization_header(context);

  ASSERT_TRUE(header.has_value());
  EXPECT_EQ(*header, "X-API-Key: " + final_key);
}
