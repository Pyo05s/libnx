// 파일: digest_auth_httpbin_test.cpp
// 생성일: 2026-02-10
// 설명: httpbin.org 기반 Digest 인증 통합 테스트

#include "common/httpbin_test_fixture.h"
#include <nxnet/auth/auth_challenge_parser.h>
#include <nxnet/auth/auth_provider.h>
#include <nxnet/auth/auth_types.h>

using namespace nx::net;
using namespace nx::net::auth;
using namespace test::httpbin;

// ============================================================================
// Digest 인증 통합 테스트
// ============================================================================

// Challenge-Response 전체 흐름 테스트 (qop=auth, MD5)
TEST_F(HttpbinIntegrationTest, DigestAuth_ChallengeResponse_QopAuth_MD5)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "httpbin.org 연결 실패: " << ec.message();
      co_return;
    }

    // Step 1: 인증 없이 요청하여 401 Challenge 받기
    std::string target =
      std::string(kDigestAuthEndpoint) + "/auth/" + kTestUser + "/" + kTestPass;
    auto challenge_response = co_await send_unauthenticated_request(target);

    if (!challenge_response.has_value()) {
      EXPECT_TRUE(false) << "Challenge 요청 실패";
      co_return;
    }
    if (challenge_response->status_code != 401) {
      EXPECT_EQ(challenge_response->status_code, 401)
        << "예상: 401, 실제: " << challenge_response->status_code;
      co_return;
    }

    // Step 2: WWW-Authenticate 헤더 파싱
    auto it = challenge_response->headers.find("WWW-Authenticate");
    if (it == challenge_response->headers.end()) {
      EXPECT_NE(it, challenge_response->headers.end()) << "WWW-Authenticate 헤더가 없음";
      co_return;
    }

    auto authenticate = std::string(it->value());

    auto challenge = AuthChallengeParser::parse(authenticate);
    if (!challenge.has_value()) {
      EXPECT_TRUE(false) << "Challenge 파싱 실패: " << challenge.error().message();
      co_return;
    }
    if (challenge->scheme != AuthScheme::kDigest) {
      EXPECT_EQ(challenge->scheme, AuthScheme::kDigest) << "Digest 스킴이 아님";
      co_return;
    }
    if (!challenge->nonce.has_value()) {
      EXPECT_TRUE(false) << "nonce가 없음";
      co_return;
    }

    // Step 3: Digest 인증 제공자 생성 및 Challenge 처리
    auto auth = AuthProviderFactory::create_digest(Credentials{kTestUser, kTestPass});
    if (!auth) {
      EXPECT_TRUE(false) << "인증 제공자 생성 실패";
      co_return;
    }

    ec = auth->process_challenge(*challenge);
    if (ec) {
      EXPECT_FALSE(true) << "Challenge 처리 실패: " << ec.message();
      co_return;
    }

    // Step 4: 인증된 요청 전송
    auto auth_response = co_await send_authenticated_request(target, auth.get());

    if (!auth_response.has_value()) {
      EXPECT_TRUE(false) << "인증 요청 실패: " << auth_response.error().message();
      co_return;
    }
    if (auth_response->status_code != 200) {
      EXPECT_EQ(auth_response->status_code, 200)
        << "Digest 인증 실패: " << auth_response->status_code;
      co_return;
    }

    // Step 5: 응답 본문 검증
    if (auth_response->body.find("\"authenticated\": true") == std::string::npos) {
      EXPECT_NE(auth_response->body.find("\"authenticated\": true"), std::string::npos)
        << "authenticated가 true가 아님";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// SHA-256 알고리즘 테스트
TEST_F(HttpbinIntegrationTest, DigestAuth_SHA256_Algorithm)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패: " << ec.message();
      co_return;
    }

    // /digest-auth/{qop}/{user}/{pass}/{algorithm}
    std::string target = std::string(kDigestAuthEndpoint) + "/auth/" + kTestUser + "/" +
                         kTestPass + "/SHA-256";

    // Challenge 받기
    auto challenge_response = co_await send_unauthenticated_request(target);
    if (!challenge_response.has_value()) {
      EXPECT_TRUE(false) << "요청 실패";
      co_return;
    }
    if (challenge_response->status_code != 401) {
      EXPECT_EQ(challenge_response->status_code, 401);
      co_return;
    }

    // Challenge 파싱
    auto it = challenge_response->headers.find("WWW-Authenticate");
    if (it == challenge_response->headers.end()) {
      EXPECT_NE(it, challenge_response->headers.end());
      co_return;
    }

    auto authenticate = std::string(it->value());

    auto challenge = AuthChallengeParser::parse(authenticate);
    if (!challenge.has_value()) {
      EXPECT_TRUE(false) << "파싱 실패";
      co_return;
    }

    // 알고리즘 확인
    if (!challenge->algorithm.has_value()) {
      EXPECT_TRUE(false) << "algorithm이 없음";
      co_return;
    }
    if (*challenge->algorithm != "SHA-256") {
      EXPECT_EQ(*challenge->algorithm, "SHA-256")
        << "예상: SHA-256, 실제: " << *challenge->algorithm;
      co_return;
    }

    // Digest 인증 제공자로 처리
    auto auth = AuthProviderFactory::create_digest(Credentials{kTestUser, kTestPass});
    ec = auth->process_challenge(*challenge);
    if (ec) {
      EXPECT_FALSE(true) << "처리 실패: " << ec.message();
      co_return;
    }

    // 인증 요청
    auto auth_response = co_await send_authenticated_request(target, auth.get());
    if (!auth_response.has_value()) {
      EXPECT_TRUE(false) << "인증 요청 실패";
      co_return;
    }
    if (auth_response->status_code != 200) {
      EXPECT_EQ(auth_response->status_code, 200) << "SHA-256 Digest 인증 실패";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// 잘못된 비밀번호 테스트
TEST_F(HttpbinIntegrationTest, DigestAuth_WrongPassword)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    std::string target =
      std::string(kDigestAuthEndpoint) + "/auth/" + kTestUser + "/" + kTestPass;

    // Challenge 받기
    auto challenge_response = co_await send_unauthenticated_request(target);
    if (!challenge_response.has_value()) {
      EXPECT_TRUE(false);
      co_return;
    }

    auto it = challenge_response->headers.find("WWW-Authenticate");
    if (it == challenge_response->headers.end()) {
      EXPECT_NE(it, challenge_response->headers.end());
      co_return;
    }

    auto authenticate = std::string(it->value());
    auto challenge = AuthChallengeParser::parse(authenticate);
    if (!challenge.has_value()) {
      EXPECT_TRUE(false);
      co_return;
    }

    // 잘못된 비밀번호로 인증 제공자 생성
    auto auth =
      AuthProviderFactory::create_digest(Credentials{kTestUser, "wrong_password"});
    ec = auth->process_challenge(*challenge);
    if (ec) {
      EXPECT_FALSE(true);
      co_return;
    }

    // 잘못된 response로 요청 → 401 예상
    auto auth_response = co_await send_authenticated_request(target, auth.get());
    if (!auth_response.has_value()) {
      EXPECT_TRUE(false);
      co_return;
    }
    if (auth_response->status_code != 401) {
      EXPECT_EQ(auth_response->status_code, 401) << "잘못된 비밀번호로 200이 반환됨";
      co_return;
    }
  };

  m_runner.run_sync(test_logic(), kTestTimeout);
}

// Nonce Count 증가 테스트 (동일 nonce로 여러 요청)
TEST_F(HttpbinIntegrationTest, DigestAuth_NonceCountIncrement)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await connect_httpbin();
    if (ec) {
      EXPECT_FALSE(true) << "연결 실패";
      co_return;
    }

    std::string target =
      std::string(kDigestAuthEndpoint) + "/auth/" + kTestUser + "/" + kTestPass;

    // Challenge 받기
    auto challenge_response = co_await send_unauthenticated_request(target);
    if (!challenge_response.has_value()) {
      EXPECT_TRUE(false);
      co_return;
    }

    auto it = challenge_response->headers.find("WWW-Authenticate");
    if (it == challenge_response->headers.end()) {
      EXPECT_NE(it, challenge_response->headers.end());
      co_return;
    }

    auto authenticate = std::string(it->value());
    auto challenge = AuthChallengeParser::parse(authenticate);
    if (!challenge.has_value()) {
      EXPECT_TRUE(false);
      co_return;
    }

    // Digest 인증 제공자 생성
    auto auth = AuthProviderFactory::create_digest(Credentials{kTestUser, kTestPass});
    ec = auth->process_challenge(*challenge);
    if (ec) {
      EXPECT_FALSE(true);
      co_return;
    }

    // 동일한 auth 객체로 여러 요청 (nc가 증가해야 함)
    for (int i = 0; i < 3; ++i) {
      auto response = co_await send_authenticated_request(target, auth.get());
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
