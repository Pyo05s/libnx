// 파일: basic_auth_httpbin_test.cpp
// 생성일: 2026-02-10
// 설명: httpbin.org 기반 Basic 인증 통합 테스트

#include "common/httpbin_test_fixture.h"
#include <nxnet/auth/auth_provider.h>
#include <nxnet/auth/auth_types.h>

#include <nlohmann/json.hpp>

using namespace nx::net;
using namespace nx::net::auth;
using namespace test::httpbin;

// ============================================================================
// Basic 인증 통합 테스트
// ============================================================================

// 정상 인증 테스트
TEST_F(HttpbinIntegrationTest, BasicAuth_Success)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    // httpbin.org 연결
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // Basic 인증 제공자 생성
    auto auth = AuthProviderFactory::create_basic(Credentials{kTestUser, kTestPass});
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    // /basic-auth/{user}/{pass} 엔드포인트로 요청
    std::string target =
      std::string(kBasicAuthEndpoint) + "/" + kTestUser + "/" + kTestPass;
    auto response = co_await send_authenticated_request(target, auth.get());

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

    // httpbin.org 응답 본문 검증
    // {"authenticated": true, "user": "testuser"}
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

      // 2. 유저명도 검증
      if (!json_body.contains("user")) {
        EXPECT_TRUE(false) << "응답 JSON에 'user' 필드가 없음";
        co_return;
      }

      EXPECT_EQ(json_body["user"].get<std::string>(), kTestUser);
    }
    catch (const nlohmann::json::parse_error& e) {
      EXPECT_TRUE(false) << "JSON 파싱 실패: " << e.what();
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 잘못된 자격 증명 테스트
TEST_F(HttpbinIntegrationTest, BasicAuth_WrongCredentials)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // 잘못된 비밀번호로 인증 제공자 생성
    auto auth =
      AuthProviderFactory::create_basic(Credentials{kTestUser, "wrong_password"});
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    // 올바른 경로로 요청 (하지만 잘못된 인증 정보라면 패스워드)
    std::string target =
      std::string(kBasicAuthEndpoint) + "/" + kTestUser + "/" + kTestPass;
    auto response = co_await send_authenticated_request(target, auth.get());

    // 401 Unauthorized 응답 확인
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 401) {
      EXPECT_EQ(response->status_code, 401)
        << "예상: 401 Unauthorized, 실제: " << response->status_code;
      co_return;
    }

    // WWW-Authenticate 헤더 확인
    auto it = response->headers.find("WWW-Authenticate");
    if (it == response->headers.end()) {
      EXPECT_NE(it, response->headers.end()) << "WWW-Authenticate 헤더가 없음";
      co_return;
    }

    auto auth_value = it->value();
    if (auth_value.find("Basic") == boost::beast::string_view::npos) {
      EXPECT_NE(auth_value.find("Basic"), boost::beast::string_view::npos)
        << "Basic 인증 Challenge가 아님";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 인증 헤더 없이 요청 (401 Challenge 확인)
TEST_F(HttpbinIntegrationTest, BasicAuth_NoAuthHeader)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // 인증 없이 요청
    std::string target =
      std::string(kBasicAuthEndpoint) + "/" + kTestUser + "/" + kTestPass;
    auto response = co_await send_unauthenticated_request(target);

    // 401 Unauthorized 응답 확인
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 401) {
      EXPECT_EQ(response->status_code, 401)
        << "예상: 401 Unauthorized, 실제: " << response->status_code;
      co_return;
    }

    // WWW-Authentic헤더 확인
    auto it = response->headers.find("WWW-Authenticate");
    if (it == response->headers.end()) {
      EXPECT_NE(it, response->headers.end()) << "WWW-Authenticate 헤더가 없음";
      co_return;
    }

    auto auth_value = it->value();
    if (auth_value.find("Basic") == boost::beast::string_view::npos) {
      EXPECT_NE(auth_value.find("Basic"), boost::beast::string_view::npos)
        << "Basic Challenge가 아님";
      co_return;
    }
    if (auth_value.find("realm=") == boost::beast::string_view::npos) {
      EXPECT_NE(auth_value.find("realm="), boost::beast::string_view::npos)
        << "realm이 없음";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 특수 문자 포함 자격 증명 테스트
TEST_F(HttpbinIntegrationTest, BasicAuth_SpecialCharacters)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // 특수 문자 포함 자격 증명
    // httpbin.org는 URL에 특수 문자를 받으므로 URL 인코딩 필요 없음
    const char* special_user = "user@domain.com";
    const char* special_pass = "p@ss:w0rd!";

    auto auth =
      AuthProviderFactory::create_basic(Credentials{special_user, special_pass});
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    // httpbin.org는 URL path에 그대로 전달
    std::string target =
      std::string(kBasicAuthEndpoint) + "/" + special_user + "/" + special_pass;
    auto response = co_await send_authenticated_request(target, auth.get());

    // 응답 검증
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200)
        << "특수 문자 포함 인증 실패: " << response->status_code;
      co_return;
    }

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

      // 2. 유저명도 검증
      if (!json_body.contains("user")) {
        EXPECT_TRUE(false) << "응답 JSON에 'user' 필드가 없음";
        co_return;
      }

      EXPECT_EQ(json_body["user"].get<std::string>(), special_user);
    }
    catch (const nlohmann::json::parse_error& e) {
      EXPECT_TRUE(false) << "JSON 파싱 실패: " << e.what();
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// hidden-basic-auth 테스트 (404 반환)
TEST_F(HttpbinIntegrationTest, BasicAuth_HiddenEndpoint)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    auto auth = AuthProviderFactory::create_basic(Credentials{kTestUser, kTestPass});

    // /hidden-basic-auth는 인증 실패 시 401 대신 404 반환
    std::string target =
      std::string(kHiddenBasicAuthEndpoint) + "/" + kTestUser + "/" + kTestPass;

    // 올바른 인증 정보로 요청
    auto response = co_await send_authenticated_request(target, auth.get());
    if (!response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (response->status_code != 200) {
      EXPECT_EQ(response->status_code, 200);
      co_return;
    }

    // 잘못된 인증 정보로 요청
    auto wrong_auth = AuthProviderFactory::create_basic(Credentials{kTestUser, "wrong"});
    auto wrong_response = co_await send_authenticated_request(target, wrong_auth.get());
    if (!wrong_response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (wrong_response->status_code != 404) {
      EXPECT_EQ(wrong_response->status_code, 404)
        << "hidden-basic-auth는 인증 실패 시 404를 반환해야 함";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// Authorization 헤더 재사용 테스트 (캐싱)
TEST_F(HttpbinIntegrationTest, BasicAuth_HeaderReuse)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패: " << ec.message();
      co_return;
    }

    auto auth = AuthProviderFactory::create_basic(Credentials{kTestUser, kTestPass});

    std::string target =
      std::string(kBasicAuthEndpoint) + "/" + kTestUser + "/" + kTestPass;

    // 첫 번째 요청
    auto response1 = co_await send_authenticated_request(target, auth.get());
    if (!response1.has_value()) {
      EXPECT_TRUE(false) << "요청 1 실패";
      co_return;
    }
    if (response1->status_code != 200) {
      EXPECT_EQ(response1->status_code, 200);
      co_return;
    }

    // 두 번째 요청 (동일한 auth 객체 재사용)
    auto response2 = co_await send_authenticated_request(target, auth.get());
    if (!response2.has_value()) {
      EXPECT_TRUE(false) << "요청 2 실패";
      co_return;
    }
    if (response2->status_code != 200) {
      EXPECT_EQ(response2->status_code, 200);
      co_return;
    }

    // 세 번째 요청
    auto response3 = co_await send_authenticated_request(target, auth.get());
    if (!response3.has_value()) {
      EXPECT_TRUE(false) << "요청 3 실패";
      co_return;
    }
    if (response3->status_code != 200) {
      EXPECT_EQ(response3->status_code, 200);
      co_return;
    }

    // 모든 요청이 성공하면 헤더 캐싱이 정상 작동
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}
