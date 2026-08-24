// 파일: ws_security_unittest.cpp
// 생성일: 2026-02-10
// 설명: WS-Security 인증 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/ws_security/ws_security_provider.h"
#include "nxnet/auth/ws_security/username_token.h"
#include "nxnet/auth/auth_types.h"
#include "nxcore/crypto/base64.h"
#include "nxcore/crypto/sha1.h"
#include "nxcore/crypto/sha256.h"
#include "nxcore/crypto/sha512.h"
#include <thread>
#include <vector>
#include <regex>

using namespace nx::net::auth;
using namespace nx::net::auth::ws_security;

// PasswordDigest 생성
TEST(WsSecurityTest, CreateWithDigest)
{
  Credentials creds{"admin", "password"};
  auto result = WsSecurityProvider::create_with_digest(creds);

  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_EQ(provider.scheme(), AuthScheme::kWsSecurity);
  EXPECT_TRUE(provider.uses_digest());
}

// PasswordText 생성
TEST(WsSecurityTest, CreateWithPlainText)
{
  Credentials creds{"user", "pass123"};
  auto result = WsSecurityProvider::create_with_plain_text(creds);

  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_EQ(provider.scheme(), AuthScheme::kWsSecurity);
  EXPECT_FALSE(provider.uses_digest());
}

// SOAP Security 헤더 생성 - Digest
TEST(WsSecurityTest, GenerateSoapHeaderDigest)
{
  Credentials creds{"admin", "password"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  auto soap_header = provider.generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());

  // XML 구조 검증
  std::string xml = *soap_header;
  EXPECT_TRUE(xml.find("<wsse:Security") != std::string::npos);
  EXPECT_TRUE(xml.find("<wsse:UsernameToken>") != std::string::npos);
  EXPECT_TRUE(xml.find("<wsse:Username>admin</wsse:Username>") != std::string::npos);
  EXPECT_TRUE(xml.find("PasswordDigest") != std::string::npos);
  EXPECT_TRUE(xml.find("<wsse:Nonce") != std::string::npos);
  EXPECT_TRUE(xml.find("<wsu:Created>") != std::string::npos);
}

// SOAP Security 헤더 생성 - PlainText
TEST(WsSecurityTest, GenerateSoapHeaderPlainText)
{
  Credentials creds{"user", "pass123"};
  auto result = WsSecurityProvider::create_with_plain_text(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  auto soap_header = provider.generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());

  // XML 구조 검증
  std::string xml = *soap_header;
  EXPECT_TRUE(xml.find("<wsse:Username>user</wsse:Username>") != std::string::npos);
  EXPECT_TRUE(xml.find("PasswordText") != std::string::npos);
  EXPECT_TRUE(xml.find("pass123") != std::string::npos);
}

// UsernameToken 직접 생성 - Digest
TEST(WsSecurityTest, UsernameTokenWithDigest)
{
  Credentials creds{"testuser", "testpass"};
  auto result = UsernameTokenBuilder::create_with_digest(creds);

  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  EXPECT_EQ(token.username, "testuser");
  EXPECT_EQ(token.password, "testpass");
  EXPECT_TRUE(token.is_digest);
  EXPECT_EQ(token.nonce_bytes.size(), 16);
  EXPECT_FALSE(token.nonce_base64.empty());
  EXPECT_FALSE(token.created.empty());
  EXPECT_FALSE(token.password_digest.empty());
}

// UsernameToken 직접 생성 - PlainText
TEST(WsSecurityTest, UsernameTokenWithPlainText)
{
  Credentials creds{"plainuser", "plainpass"};
  auto result = UsernameTokenBuilder::create_with_plain_text(creds);

  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  EXPECT_EQ(token.username, "plainuser");
  EXPECT_FALSE(token.is_digest);
  EXPECT_EQ(token.nonce_bytes.size(), 16);
}

// PasswordDigest 검증
TEST(WsSecurityTest, PasswordDigestVerification)
{
  Credentials creds{"admin", "password"};
  auto result = UsernameTokenBuilder::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  // 수동으로 PasswordDigest 계산
  nx::crypto::Bytes combined;
  combined.insert(combined.end(), token.nonce_bytes.begin(), token.nonce_bytes.end());
  combined.insert(combined.end(), token.created.begin(), token.created.end());
  combined.insert(combined.end(), token.password.begin(), token.password.end());

  auto hash_result = nx::crypto::Sha1::hash(combined);
  EXPECT_FALSE(hash_result.empty());

  std::string expected_digest = nx::crypto::Base64::encode(hash_result);

  EXPECT_EQ(token.password_digest, expected_digest);
}

// Nonce 유일성 검증
TEST(WsSecurityTest, NonceUniqueness)
{
  Credentials creds{"user", "pass"};

  std::vector<std::string> nonces;
  for (int i = 0; i < 100; ++i) {
    auto result = UsernameTokenBuilder::create_with_digest(creds);
    ASSERT_TRUE(result.has_value());
    nonces.push_back(result->nonce_base64);
  }

  // 모든 nonce가 고유한지 확인
  std::sort(nonces.begin(), nonces.end());
  auto unique_end = std::unique(nonces.begin(), nonces.end());
  EXPECT_EQ(std::distance(nonces.begin(), unique_end), 100);
}

// ISO 8601 타임스탬프 형식 검증
TEST(WsSecurityTest, TimestampFormat)
{
  Credentials creds{"user", "pass"};
  auto result = UsernameTokenBuilder::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  // ISO 8601 형식: YYYY-MM-DDTHH:MM:SS.sssZ (밀리초 포함)
  std::regex iso8601_pattern(R"(\d{4}-\d{2}-\d{2}T\d{2}:\d{2}:\d{2}\.\d{3}Z)");
  EXPECT_TRUE(std::regex_match(token.created, iso8601_pattern));
}

// 빈 사용자명 오류
TEST(WsSecurityTest, EmptyUsernameReturnsError)
{
  Credentials creds{"", "password"};
  auto result = WsSecurityProvider::create_with_digest(creds);

  EXPECT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidCredentials));
}

// 자격 증명 갱신
TEST(WsSecurityTest, UpdateCredentials)
{
  Credentials creds{"olduser", "oldpass"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_EQ(provider.get_credentials().username, "olduser");

  // 자격 증명 갱신
  Credentials new_creds{"newuser", "newpass"};
  auto ec = provider.update_credentials(new_creds);
  EXPECT_FALSE(ec);

  EXPECT_EQ(provider.get_credentials().username, "newuser");
  EXPECT_EQ(provider.get_credentials().password, "newpass");
}

// 빈 사용자명으로 갱신 시 오류
TEST(WsSecurityTest, UpdateWithEmptyUsernameReturnsError)
{
  Credentials creds{"validuser", "validpass"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  Credentials invalid_creds{"", "newpass"};
  auto ec = provider.update_credentials(invalid_creds);

  EXPECT_TRUE(ec);
  EXPECT_EQ(ec, make_error_code(AuthError::kInvalidCredentials));

  // 기존 자격 증명 유지 확인
  EXPECT_EQ(provider.get_credentials().username, "validuser");
}

// Authorization 헤더는 지원하지 않음
TEST(WsSecurityTest, AuthorizationHeaderNotSupported)
{
  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  AuthContext context{"GET", "/api"};
  auto header = provider.generate_authorization_header(context);

  EXPECT_FALSE(header.has_value());
}

// 복제 - Digest
TEST(WsSecurityTest, CloneDigest)
{
  Credentials creds{"original", "password"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();
  auto cloned = original.clone();

  ASSERT_NE(cloned, nullptr);
  EXPECT_EQ(cloned->scheme(), AuthScheme::kWsSecurity);

  auto soap_header = cloned->generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());
  EXPECT_TRUE(soap_header->find("original") != std::string::npos);
}

// 복제 - PlainText
TEST(WsSecurityTest, ClonePlainText)
{
  Credentials creds{"plainuser", "plainpass"};
  auto result = WsSecurityProvider::create_with_plain_text(creds);
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();
  auto cloned = original.clone();

  ASSERT_NE(cloned, nullptr);

  auto soap_header = cloned->generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());
  EXPECT_TRUE(soap_header->find("PasswordText") != std::string::npos);
}

// 복제 후 독립성
TEST(WsSecurityTest, CloneIndependence)
{
  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto original = std::move(result.value());
  auto cloned = original->clone();

  // 원본 삭제
  original.reset();

  // 복제본은 독립적으로 동작
  auto soap_header = cloned->generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());
}

// 팩토리 메서드 - Digest
TEST(WsSecurityTest, FactoryCreateDigest)
{
  Credentials creds{"factoryuser", "factorypass"};
  auto provider = AuthProviderFactory::create_ws_security(creds, true);

  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->scheme(), AuthScheme::kWsSecurity);

  auto soap_header = provider->generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());
  EXPECT_TRUE(soap_header->find("PasswordDigest") != std::string::npos);
}

// 팩토리 메서드 - PlainText
TEST(WsSecurityTest, FactoryCreatePlainText)
{
  Credentials creds{"plainuser", "plainpass"};
  auto provider = AuthProviderFactory::create_ws_security(creds, false);

  ASSERT_NE(provider, nullptr);

  auto soap_header = provider->generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());
  EXPECT_TRUE(soap_header->find("PasswordText") != std::string::npos);
}

// OASIS 네임스페이스 검증
TEST(WsSecurityTest, OasisNamespaces)
{
  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  auto soap_header = provider.generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());

  std::string xml = *soap_header;
  EXPECT_TRUE(
    xml.find(
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-wssecurity-secext-1.0.xsd")
    != std::string::npos);
  EXPECT_TRUE(
    xml.find(
      "http://docs.oasis-open.org/wss/2004/01/"
      "oasis-200401-wss-wssecurity-utility-1.0.xsd")
    != std::string::npos);
}

// 멀티스레드 안전성: 자격 증명 갱신
TEST(WsSecurityTest, ConcurrentCredentialsUpdate)
{
  Credentials creds{"initial", "initial"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
  const int num_threads = 10;
  const int updates_per_thread = 50;

  std::vector<std::thread> threads;

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, i, updates_per_thread]() {
      for (int j = 0; j < updates_per_thread; ++j) {
        Credentials new_creds{
          "user_" + std::to_string(i) + "_" + std::to_string(j),
          "pass_" + std::to_string(i) + "_" + std::to_string(j)};
        (void)provider.update_credentials(new_creds);
      }
    });
  }

  for (auto& thread : threads) {
    thread.join();
  }

  // 최종 자격 증명이 유효한지 확인
  auto final_creds = provider.get_credentials();
  EXPECT_FALSE(final_creds.username.empty());
  EXPECT_TRUE(final_creds.username.starts_with("user_"));
}

// 멀티스레드 안전성: SOAP 헤더 생성
TEST(WsSecurityTest, ConcurrentSoapHeaderGeneration)
{
  Credentials creds{"concurrent", "password"};
  auto result = WsSecurityProvider::create_with_digest(creds);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();
  const int num_threads = 20;
  const int requests_per_thread = 50;

  std::vector<std::thread> threads;
  std::atomic<int> success_count{0};

  for (int i = 0; i < num_threads; ++i) {
    threads.emplace_back([&provider, &success_count, requests_per_thread]() {
      for (int j = 0; j < requests_per_thread; ++j) {
        auto soap_header = provider.generate_soap_security_header();
        if (
          soap_header.has_value()
          && soap_header->find("concurrent") != std::string::npos) {
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

// SHA-256 알고리즘 지원
TEST(WsSecurityTest, Sha256Algorithm)
{
  using namespace ws_security;

  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds, HashAlgorithm::kSha256);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_TRUE(provider.uses_digest());
  EXPECT_EQ(provider.get_algorithm(), HashAlgorithm::kSha256);

  auto soap_header = provider.generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());

  // SOAP 헤더에 사용자명 포함 확인
  EXPECT_NE(soap_header->find("user"), std::string::npos);
  EXPECT_NE(soap_header->find("PasswordDigest"), std::string::npos);
}

// SHA-1과 SHA-256 결과가 다름을 확인
TEST(WsSecurityTest, Sha1VsSha256Different)
{
  using namespace ws_security;

  Credentials creds{"testuser", "testpass"};

  // SHA-1 Digest
  auto token_sha1_result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha1);
  ASSERT_TRUE(token_sha1_result.has_value());
  auto& token_sha1 = *token_sha1_result;

  // SHA-256 Digest
  auto token_sha256_result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha256);
  ASSERT_TRUE(token_sha256_result.has_value());
  auto& token_sha256 = *token_sha256_result;

  // 두 알고리즘의 결과는 달라야 함
  EXPECT_NE(token_sha1.password_digest, token_sha256.password_digest);

  // SHA-256이 더 긴 해시 생성 (32바이트 vs 20바이트)
  // Base64로 인코딩하면 SHA-1: 28자, SHA-256: 44자
  EXPECT_GT(
    token_sha256.password_digest.length(),
    token_sha1.password_digest.length());
}

// SHA-256 PasswordDigest 수동 검증
TEST(WsSecurityTest, Sha256PasswordDigestVerification)
{
  using namespace ws_security;

  Credentials creds{"admin", "secret"};
  auto result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha256);
  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  // 수동으로 PasswordDigest 계산
  nx::crypto::Bytes combined;
  combined.insert(combined.end(), token.nonce_bytes.begin(), token.nonce_bytes.end());
  combined.insert(combined.end(), token.created.begin(), token.created.end());
  combined.insert(combined.end(), token.password.begin(), token.password.end());

  auto hash_result = nx::crypto::Sha256::hash(combined);
  EXPECT_FALSE(hash_result.empty());

  std::string expected_digest = nx::crypto::Base64::encode(hash_result);

  // 생성된 Digest와 수동 계산 결과가 일치해야 함
  EXPECT_EQ(token.password_digest, expected_digest);
}

// SHA-256 Provider Clone
TEST(WsSecurityTest, CloneSha256)
{
  using namespace ws_security;

  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds, HashAlgorithm::kSha256);
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();

  auto cloned = original.clone();
  ASSERT_NE(cloned, nullptr);

  auto cloned_ws = dynamic_cast<WsSecurityProvider*>(cloned.get());
  ASSERT_NE(cloned_ws, nullptr);

  EXPECT_TRUE(cloned_ws->uses_digest());
  EXPECT_EQ(cloned_ws->get_algorithm(), HashAlgorithm::kSha256);
  EXPECT_EQ(cloned_ws->get_credentials().username, "user");
}

// SHA-512 알고리즘 지원
TEST(WsSecurityTest, Sha512Algorithm)
{
  using namespace ws_security;

  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds, HashAlgorithm::kSha512);
  ASSERT_TRUE(result.has_value());
  auto& provider = *result.value();

  EXPECT_TRUE(provider.uses_digest());
  EXPECT_EQ(provider.get_algorithm(), HashAlgorithm::kSha512);

  auto soap_header = provider.generate_soap_security_header();
  ASSERT_TRUE(soap_header.has_value());

  // SOAP 헤더에 사용자명 포함 확인
  EXPECT_NE(soap_header->find("user"), std::string::npos);
  EXPECT_NE(soap_header->find("PasswordDigest"), std::string::npos);
}

// SHA-512 PasswordDigest 수동 검증
TEST(WsSecurityTest, Sha512PasswordDigestVerification)
{
  using namespace ws_security;

  Credentials creds{"admin", "secret"};
  auto result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha512);
  ASSERT_TRUE(result.has_value());
  const auto& token = *result;

  // 수동으로 PasswordDigest 계산
  nx::crypto::Bytes combined;
  combined.insert(combined.end(), token.nonce_bytes.begin(), token.nonce_bytes.end());
  combined.insert(combined.end(), token.created.begin(), token.created.end());
  combined.insert(combined.end(), token.password.begin(), token.password.end());

  auto hash_result = nx::crypto::Sha512::hash(combined);
  EXPECT_FALSE(hash_result.empty());

  std::string expected_digest = nx::crypto::Base64::encode(hash_result);

  // 생성된 Digest와 수동 계산 결과가 일치해야 함
  EXPECT_EQ(token.password_digest, expected_digest);
}

// 모든 알고리즘 결과 비교
TEST(WsSecurityTest, AllAlgorithmsDifferent)
{
  using namespace ws_security;

  Credentials creds{"testuser", "testpass"};

  // SHA-1 Digest
  auto token_sha1_result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha1);
  ASSERT_TRUE(token_sha1_result.has_value());
  auto& token_sha1 = *token_sha1_result;

  // SHA-256 Digest
  auto token_sha256_result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha256);
  ASSERT_TRUE(token_sha256_result.has_value());
  auto& token_sha256 = *token_sha256_result;

  // SHA-512 Digest
  auto token_sha512_result
    = UsernameTokenBuilder::create_with_digest(creds, HashAlgorithm::kSha512);
  ASSERT_TRUE(token_sha512_result.has_value());
  auto& token_sha512 = *token_sha512_result;

  // 세 알고리즘의 결과는 모두 달라야 함
  EXPECT_NE(token_sha1.password_digest, token_sha256.password_digest);
  EXPECT_NE(token_sha1.password_digest, token_sha512.password_digest);
  EXPECT_NE(token_sha256.password_digest, token_sha512.password_digest);

  // 해시 길이 비교 (Base64 인코딩)
  // SHA-1: 20바이트 -> ~28자
  // SHA-256: 32바이트 -> ~44자
  // SHA-512: 64바이트 -> ~88자
  EXPECT_LT(
    token_sha1.password_digest.length(),
    token_sha256.password_digest.length());
  EXPECT_LT(
    token_sha256.password_digest.length(),
    token_sha512.password_digest.length());
}

// SHA-512 Provider Clone
TEST(WsSecurityTest, CloneSha512)
{
  using namespace ws_security;

  Credentials creds{"user", "pass"};
  auto result = WsSecurityProvider::create_with_digest(creds, HashAlgorithm::kSha512);
  ASSERT_TRUE(result.has_value());
  auto& original = *result.value();

  auto cloned = original.clone();
  ASSERT_NE(cloned, nullptr);

  auto cloned_ws = dynamic_cast<WsSecurityProvider*>(cloned.get());
  ASSERT_NE(cloned_ws, nullptr);

  EXPECT_TRUE(cloned_ws->uses_digest());
  EXPECT_EQ(cloned_ws->get_algorithm(), HashAlgorithm::kSha512);
  EXPECT_EQ(cloned_ws->get_credentials().username, "user");
}
