// 파일: apikey_auth_httpbin_test.cpp
// 생성일: 2026-02-10
// 설명: httpbin.org 기반 API Key 통합 테스트

#include "common/httpbin_test_fixture.h"
#include <nxnet/auth/auth_provider.h>
#include <nxnet/auth/auth_types.h>

using namespace nx::net;
using namespace nx::net::auth;
using namespace test::httpbin;

// ============================================================================
// API Key 통합 테스트
// ============================================================================

// X-API-Key 헤더로 전송 테스트
TEST_F(HttpbinIntegrationTest, ApiKeyAuth_XApiKeyHeader)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "{} 연결 실패: " << kHttpbinHost << ec.message();
      co_return;
    }

    // API Key 생성 (기본 헤더: X-API-Key)
    std::string api_key = "test_api_key_12345";
    auto auth = AuthProviderFactory::create_api_key(api_key, "X-API-Key");
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    // /headers 엔드포인트로 요청 (헤더 반환)
    auto response = co_await send_authenticated_request(kHeadersEndpoint, auth.get());

    // 응답 검증
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패: " << response.error().message();
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200)
        << "예상: 200 OK, 실제: " << response->status_code;
      co_return;
    }

    // httpbin.org는 {"headers": {"X-Api-Key": "..."}} 형식으로 반환
    if (response->body.find("X-Api-Key") == std::string::npos) {
      EXPECT_NE(response->body.find("X-Api-Key"), std::string::npos)
        << "X-API-Key 헤더가 응답에 없음";
      co_return;
    }
    if (response->body.find(api_key) == std::string::npos) {
      EXPECT_NE(response->body.find(api_key), std::string::npos)
        << "API Key 값이 응답에 없음";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 커스텀 헤더명 사용 테스트
TEST_F(HttpbinIntegrationTest, ApiKeyAuth_CustomHeaderName)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    // 커스텀 헤더명 사용
    std::string api_key = "custom_key_789";
    std::string header_name = "X-Custom-Api-Token";
    auto auth = AuthProviderFactory::create_api_key(api_key, header_name);

    auto response = co_await send_authenticated_request(kHeadersEndpoint, auth.get());

    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200);
      co_return;
    }

    // 커스텀 헤더명과 값 확인
    if (response->body.find("X-Custom-Api-Token") == std::string::npos) {
      EXPECT_NE(response->body.find("X-Custom-Api-Token"), std::string::npos)
        << "커스텀 헤더가 응답에 없음";
      co_return;
    }
    if (response->body.find(api_key) == std::string::npos) {
      EXPECT_NE(response->body.find(api_key), std::string::npos);
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// Authorization 헤더로 전송 (Bearer 스타일)
TEST_F(HttpbinIntegrationTest, ApiKeyAuth_AuthorizationHeader)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    // Authorization 헤더로 전송 (일부 API는 이 방식 사용)
    std::string api_key = "auth_header_key_456";
    auto auth = AuthProviderFactory::create_api_key(api_key, "Authorization");

    auto response = co_await send_authenticated_request(kHeadersEndpoint, auth.get());

    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200);
      co_return;
    }

    // Authorization 헤더 확인
    if (response->body.find("Authorization") == std::string::npos) {
      EXPECT_NE(response->body.find("Authorization"), std::string::npos);
      co_return;
    }
    if (response->body.find(api_key) == std::string::npos) {
      EXPECT_NE(response->body.find(api_key), std::string::npos);
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 특수 문자 포함 API Key 테스트
TEST_F(HttpbinIntegrationTest, ApiKeyAuth_SpecialCharacters)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    // 특수 문자 포함 API Key
    std::string api_key = "key-with_special.chars!@#$%";
    auto auth = AuthProviderFactory::create_api_key(api_key, "X-API-Key");

    auto response = co_await send_authenticated_request(kHeadersEndpoint, auth.get());

    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200);
      co_return;
    }

    // 특수 문자가 그대로 전송되었는지 확인
    if (response->body.find("X-Api-Key") == std::string::npos) {
      EXPECT_NE(response->body.find("X-Api-Key"), std::string::npos);
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 헤더 재사용 테스트
TEST_F(HttpbinIntegrationTest, ApiKeyAuth_HeaderReuse)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    std::string api_key = "reusable_api_key";
    auto auth = AuthProviderFactory::create_api_key(api_key, "X-API-Key");

    // 동일한 헤더로 여러 요청
    for (int i = 0; i < 3; ++i) {
      auto response = co_await send_authenticated_request(kHeadersEndpoint, auth.get());

      if (!response.has_value()) {
        EXPECT_TRUE(false) << "요청 " << (i + 1) << " 실패";
        co_return;
      }
      if (response->status_code != 200) {
        EXPECT_EQ(response->status_code, 200) << "요청 " << (i + 1) << " 실패";
        co_return;
      }
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}
