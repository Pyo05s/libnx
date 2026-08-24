// 파일: bearer_auth_httpbin_test.cpp
// 생성일: 2026-02-10
// 설명: httpbin.org 기반 Bearer 토큰 통합 테스트

#include "common/httpbin_test_fixture.h"
#include <nxnet/auth/auth_provider.h>
#include <nxnet/auth/auth_types.h>

#include <nlohmann/json.hpp>

using namespace nx::net;
using namespace nx::net::auth;
using namespace test::httpbin;

// ============================================================================
// Bearer 토큰 통합 테스트
// ============================================================================

// 유효한 토큰 헤더 전송 테스트
TEST_F(HttpbinIntegrationTest, BearerAuth_ValidToken)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // Bearer 토큰 생성 (httpbin.org는 토큰 검증 없음)
    std::string test_token = "test_bearer_token_12345";
    auto auth = AuthProviderFactory::create_bearer(test_token);
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    // /bearer 엔드포인트로 요청
    auto response = co_await send_authenticated_request(kBearerEndpoint, auth.get());

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

    // httpbin.org는 {"authenticated": true, "token": "..."} 반환
    try {
      // 응답 본문을 JSON 객체로 파싱
      auto json_body = nlohmann::json::parse(response->body);

      // 1. "authenticated" 필드가 존재하고 true인지 검증
      if (!json_body.contains("authenticated")) {
        EXPECT_TRUE(false) << "응답 JSON에 'authenticated' 필드가 없음";
        co_return;
      }

      EXPECT_TRUE(json_body["authenticated"].get<bool>())
        << "authenticated 필드가 true가 아님";

      // 2. token 검증
      if (!json_body.contains("token")) {
        EXPECT_TRUE(false) << "응답 JSON에 'token' 필드가 없음";
        co_return;
      }

      EXPECT_EQ(json_body["token"].get<std::string>(), test_token);
    }
    catch (const nlohmann::json::parse_error& e) {
      EXPECT_TRUE(false) << "JSON 파싱 실패: " << e.what();
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// Authorization 헤더 없이 요청 (401 확인)
TEST_F(HttpbinIntegrationTest, BearerAuth_NoAuthHeader)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    // 인증 없이 /bearer 요청
    auto response = co_await send_unauthenticated_request(kBearerEndpoint);

    // 401 Unauthorized 확인
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 401) {
      EXPECT_EQ(response->status_code, 401)
        << "예상: 401 Unauthorized, 실제: " << response->status_code;
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// JWT 형식 토큰 테스트 (검증 없음, 전송만)
TEST_F(HttpbinIntegrationTest, BearerAuth_JwtFormatToken)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    // JWT 형식 토큰 (실제 서명 검증 없음, httpbin.org는 형식만 확인)
    std::string jwt_token =
      "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9."
      "eyJzdWIiOiIxMjM0NTY3ODkwIiwibmFtZSI6IkpvaG4gRG9lIiwiaWF0IjoxNTE2MjM5M"
      "DIyfQ."
      "SflKxwRJSMeKKF2QT4fwpMeJf36POk6yJV_adQssw5c";

    auto auth = AuthProviderFactory::create_bearer(jwt_token);
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    auto response = co_await send_authenticated_request(kBearerEndpoint, auth.get());

    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200) << "JWT 형식 토큰 전송 실패";
      co_return;
    }

    // 토큰이 그대로 반환되는지 확인
    if (response->body.find(jwt_token) == std::string::npos) {
      EXPECT_NE(response->body.find(jwt_token), std::string::npos)
        << "JWT 토큰이 응답에 포함되지 않음";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// Bearer 토큰 재사용 테스트
TEST_F(HttpbinIntegrationTest, BearerAuth_TokenReuse)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    std::string token = "reusable_token_456";
    auto auth = AuthProviderFactory::create_bearer(token);

    // 동일한 토큰으로 여러 요청
    for (int i = 0; i < 3; ++i) {
      auto response = co_await send_authenticated_request(kBearerEndpoint, auth.get());

      if (!response.has_value()) {
        EXPECT_TRUE(false) << "요청 " << (i + 1) << " 실패";
        co_return;
      }
      if (response->status_code != 200) {
        EXPECT_EQ(response->status_code, 200) << "요청 " << (i + 1) << " 인증 실패";
        co_return;
      }
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}
