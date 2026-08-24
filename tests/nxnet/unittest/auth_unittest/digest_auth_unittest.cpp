// 파일: digest_auth_unittest.cpp
// 생성일: 2026-02-10
// 설명: Digest 인증 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/digest/digest_auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include "nxnet/auth/auth_challenge_parser.h"

using namespace nx::net::auth;

// RFC 7616 예제 검증
TEST(DigestAuthTest, Rfc7616Example)
{
  Credentials creds{"Mufasa", "Circle Of Life"};
  DigestAuthProvider provider(creds);

  // Challenge 설정
  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "testrealm@host.com";
  challenge.nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
  challenge.qop = "auth";
  challenge.opaque = "5ccc069c403ebaf9f0171e9517f40e41";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  // Authorization 헤더 생성
  AuthContext context{"GET", "/dir/index.html"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());

  // 헤더 필드 확인
  EXPECT_NE(result->find("Digest username=\"Mufasa\""), std::string::npos);
  EXPECT_NE(result->find("realm=\"testrealm@host.com\""), std::string::npos);
  EXPECT_NE(result->find("uri=\"/dir/index.html\""), std::string::npos);
  EXPECT_NE(result->find("qop=auth"), std::string::npos);
  EXPECT_NE(result->find("nc=00000001"), std::string::npos);
  EXPECT_NE(result->find("response=\""), std::string::npos);
}

// Challenge 파싱 및 처리
TEST(DigestAuthTest, ProcessChallenge)
{
  Credentials creds{"user", "password"};
  DigestAuthProvider provider(creds);

  std::string www_auth = "Digest realm=\"test\", nonce=\"abc123\", qop=\"auth\"";
  auto challenge_result = AuthChallengeParser::parse(www_auth);

  ASSERT_TRUE(challenge_result.has_value());

  auto error = provider.process_challenge(*challenge_result);
  EXPECT_FALSE(error);
}

// QoP 없는 경우
TEST(DigestAuthTest, WithoutQop)
{
  Credentials creds{"testuser", "testpass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "simple";
  challenge.nonce = "nonce123";
  // qop 없음

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"GET", "/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());

  // QoP 관련 필드가 없어야 함
  EXPECT_EQ(result->find("qop="), std::string::npos);
  EXPECT_EQ(result->find("nc="), std::string::npos);
  EXPECT_EQ(result->find("cnonce="), std::string::npos);
}

// Nonce Count 증가 확인
TEST(DigestAuthTest, NonceCountIncrement)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "abc";
  challenge.qop = "auth";

  provider.process_challenge(challenge);

  AuthContext context{"GET", "/1"};

  // 첫 번째 요청
  auto result1 = provider.generate_authorization_header(context);
  ASSERT_TRUE(result1.has_value());
  EXPECT_NE(result1->find("nc=00000001"), std::string::npos);

  // 두 번째 요청
  auto result2 = provider.generate_authorization_header(context);
  ASSERT_TRUE(result2.has_value());
  EXPECT_NE(result2->find("nc=00000002"), std::string::npos);

  // 세 번째 요청
  auto result3 = provider.generate_authorization_header(context);
  ASSERT_TRUE(result3.has_value());
  EXPECT_NE(result3->find("nc=00000003"), std::string::npos);
}

// Challenge 없이 헤더 생성 시도
TEST(DigestAuthTest, NoChallengeError)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  // Challenge 없음
  AuthContext context{"GET", "/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kNoChallenge));
}

// 잘못된 Challenge
TEST(DigestAuthTest, InvalidChallenge)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kBasic; // 잘못된 스킴
  challenge.realm = "test";
  challenge.nonce = "abc";

  auto error = provider.process_challenge(challenge);
  EXPECT_TRUE(error);
  EXPECT_EQ(error, make_error_code(AuthError::kInvalidChallenge));
}

// Nonce 없는 Challenge
TEST(DigestAuthTest, MissingNonce)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  // nonce 없음

  auto error = provider.process_challenge(challenge);
  EXPECT_TRUE(error);
  EXPECT_EQ(error, make_error_code(AuthError::kMissingParameter));
}

// Algorithm 지정
TEST(DigestAuthTest, WithAlgorithm)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "abc123";
  challenge.algorithm = "MD5";

  provider.process_challenge(challenge);

  AuthContext context{"GET", "/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=MD5"), std::string::npos);
}

// Opaque 필드
TEST(DigestAuthTest, WithOpaque)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "nonce";
  challenge.opaque = "opaque123";

  provider.process_challenge(challenge);

  AuthContext context{"GET", "/api"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("opaque=\"opaque123\""), std::string::npos);
}

// 스킴 확인
TEST(DigestAuthTest, SchemeIdentification)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  EXPECT_EQ(provider.scheme(), AuthScheme::kDigest);
}

// Clone 테스트
TEST(DigestAuthTest, CloneProvider)
{
  Credentials creds{"original", "password"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "nonce";

  provider.process_challenge(challenge);

  auto cloned = provider.clone();
  ASSERT_NE(cloned, nullptr);

  AuthContext context{"GET", "/"};
  auto result = cloned->generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
}

// 팩토리를 통한 생성
TEST(DigestAuthTest, FactoryCreation)
{
  Credentials creds{"factory", "test"};
  auto provider = AuthProviderFactory::create_digest(creds);

  ASSERT_NE(provider, nullptr);
  EXPECT_EQ(provider->scheme(), AuthScheme::kDigest);
}

// 여러 URI로 요청
TEST(DigestAuthTest, DifferentUris)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "api";
  challenge.nonce = "xyz";
  challenge.qop = "auth";

  provider.process_challenge(challenge);

  AuthContext context1{"GET", "/api/users"};
  auto result1 = provider.generate_authorization_header(context1);
  ASSERT_TRUE(result1.has_value());
  EXPECT_NE(result1->find("uri=\"/api/users\""), std::string::npos);

  AuthContext context2{"POST", "/api/data"};
  auto result2 = provider.generate_authorization_header(context2);
  ASSERT_TRUE(result2.has_value());
  EXPECT_NE(result2->find("uri=\"/api/data\""), std::string::npos);
}

// SHA-256 알고리즘 사용 (RFC 7616 Section 3.4)
TEST(DigestAuthTest, Sha256Algorithm)
{
  Credentials creds{"user", "password"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "secure";
  challenge.nonce = "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v";
  challenge.algorithm = "SHA-256";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"GET", "/protected/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=SHA-256"), std::string::npos);
  EXPECT_NE(result->find("response=\""), std::string::npos);
}

// SHA-256 알고리즘 대소문자 구분 없이 처리
TEST(DigestAuthTest, Sha256CaseInsensitive)
{
  Credentials creds{"testuser", "testpass"};
  DigestAuthProvider provider(creds);

  // 소문자로 지정
  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "abc123";
  challenge.algorithm = "sha-256";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"POST", "/api/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  // 헤더에는 원본 그대로 포함
  EXPECT_NE(result->find("algorithm=sha-256"), std::string::npos);
}

// MD5와 SHA-256 응답 차이 확인
TEST(DigestAuthTest, Md5VsSha256Different)
{
  Credentials creds{"user", "pass"};

  // MD5 사용
  DigestAuthProvider provider_md5(creds);
  AuthChallenge challenge_md5;
  challenge_md5.scheme = AuthScheme::kDigest;
  challenge_md5.realm = "test";
  challenge_md5.nonce = "nonce123";
  challenge_md5.algorithm = "MD5";
  challenge_md5.qop = "auth";
  provider_md5.process_challenge(challenge_md5);

  // SHA-256 사용
  DigestAuthProvider provider_sha256(creds);
  AuthChallenge challenge_sha256;
  challenge_sha256.scheme = AuthScheme::kDigest;
  challenge_sha256.realm = "test";
  challenge_sha256.nonce = "nonce123";
  challenge_sha256.algorithm = "SHA-256";
  challenge_sha256.qop = "auth";
  provider_sha256.process_challenge(challenge_sha256);

  AuthContext context{"GET", "/resource"};

  auto result_md5 = provider_md5.generate_authorization_header(context);
  auto result_sha256 = provider_sha256.generate_authorization_header(context);

  ASSERT_TRUE(result_md5.has_value());
  ASSERT_TRUE(result_sha256.has_value());

  // 두 응답이 달라야 함 (해시 알고리즘이 다르므로)
  EXPECT_NE(*result_md5, *result_sha256);
}

// 알고리즘 명시 없을 때 SHA-256 기본값 사용
TEST(DigestAuthTest, DefaultAlgorithmSha256)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "default";
  challenge.nonce = "xyz";
  // algorithm 필드 없음 (기본값 SHA-256)

  provider.process_challenge(challenge);

  AuthContext context{"GET", "/"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  // 알고리즘이 명시되지 않으면 SHA-256이 기본값 (보안 강화)
}
// SHA-512 알고리즘 사용
TEST(DigestAuthTest, Sha512Algorithm)
{
  Credentials creds{"user", "password"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "secure";
  challenge.nonce = "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v";
  challenge.algorithm = "SHA-512";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"GET", "/protected/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=SHA-512"), std::string::npos);
  EXPECT_NE(
    result->find(
      "response="
      ""),
    std::string::npos);
}

// SHA-512-256 알고리즘 사용 (RFC 7616)
TEST(DigestAuthTest, Sha512_256Algorithm)
{
  Credentials creds{"testuser", "testpass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "abc123def456";
  challenge.algorithm = "SHA-512-256";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"POST", "/api/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=SHA-512-256"), std::string::npos);
}

// 모든 알고리즘 응답 차이 확인
TEST(DigestAuthTest, AllAlgorithmsDifferent)
{
  Credentials creds{"user", "pass"};
  std::string realm = "test";
  std::string nonce = "nonce123";
  std::string qop = "auth";

  // MD5
  DigestAuthProvider provider_md5(creds);
  AuthChallenge challenge_md5;
  challenge_md5.scheme = AuthScheme::kDigest;
  challenge_md5.realm = realm;
  challenge_md5.nonce = nonce;
  challenge_md5.algorithm = "MD5";
  challenge_md5.qop = qop;
  provider_md5.process_challenge(challenge_md5);

  // SHA-256
  DigestAuthProvider provider_sha256(creds);
  AuthChallenge challenge_sha256;
  challenge_sha256.scheme = AuthScheme::kDigest;
  challenge_sha256.realm = realm;
  challenge_sha256.nonce = nonce;
  challenge_sha256.algorithm = "SHA-256";
  challenge_sha256.qop = qop;
  provider_sha256.process_challenge(challenge_sha256);

  // SHA-512
  DigestAuthProvider provider_sha512(creds);
  AuthChallenge challenge_sha512;
  challenge_sha512.scheme = AuthScheme::kDigest;
  challenge_sha512.realm = realm;
  challenge_sha512.nonce = nonce;
  challenge_sha512.algorithm = "SHA-512";
  challenge_sha512.qop = qop;
  provider_sha512.process_challenge(challenge_sha512);

  // SHA-512-256
  DigestAuthProvider provider_sha512_256(creds);
  AuthChallenge challenge_sha512_256;
  challenge_sha512_256.scheme = AuthScheme::kDigest;
  challenge_sha512_256.realm = realm;
  challenge_sha512_256.nonce = nonce;
  challenge_sha512_256.algorithm = "SHA-512-256";
  challenge_sha512_256.qop = qop;
  provider_sha512_256.process_challenge(challenge_sha512_256);

  AuthContext context{"GET", "/resource"};

  auto result_md5 = provider_md5.generate_authorization_header(context);
  auto result_sha256 = provider_sha256.generate_authorization_header(context);
  auto result_sha512 = provider_sha512.generate_authorization_header(context);
  auto result_sha512_256 = provider_sha512_256.generate_authorization_header(context);

  ASSERT_TRUE(result_md5.has_value());
  ASSERT_TRUE(result_sha256.has_value());
  ASSERT_TRUE(result_sha512.has_value());
  ASSERT_TRUE(result_sha512_256.has_value());

  // 모든 응답이 달라야 함
  EXPECT_NE(*result_md5, *result_sha256);
  EXPECT_NE(*result_md5, *result_sha512);
  EXPECT_NE(*result_md5, *result_sha512_256);
  EXPECT_NE(*result_sha256, *result_sha512);
  EXPECT_NE(*result_sha256, *result_sha512_256);
  EXPECT_NE(*result_sha512, *result_sha512_256);
}

// SHA-256-sess 알고리즘 사용 (RFC 7616)
TEST(DigestAuthTest, Sha256SessAlgorithm)
{
  Credentials creds{"user", "password"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "secure";
  challenge.nonce = "7ypf/xlj9XXwfDPEoM4URrv/xwf94BcCAzFZH4GiTo0v";
  challenge.algorithm = "SHA-256-sess";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"GET", "/protected/resource"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=SHA-256-sess"), std::string::npos);
  EXPECT_NE(result->find("response=\""), std::string::npos);
}

// SHA-512-256-sess 알고리즘 사용 (RFC 7616)
TEST(DigestAuthTest, Sha512_256SessAlgorithm)
{
  Credentials creds{"testuser", "testpass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "abc123def456";
  challenge.algorithm = "SHA-512-256-sess";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"POST", "/api/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=SHA-512-256-sess"), std::string::npos);
}

// sess와 비-sess 알고리즘 응답 차이 확인
TEST(DigestAuthTest, SessVsNonSessDifferent)
{
  Credentials creds{"user", "pass"};

  // SHA-256 사용
  DigestAuthProvider provider_sha256(creds);
  AuthChallenge challenge_sha256;
  challenge_sha256.scheme = AuthScheme::kDigest;
  challenge_sha256.realm = "test";
  challenge_sha256.nonce = "nonce123";
  challenge_sha256.algorithm = "SHA-256";
  challenge_sha256.qop = "auth";
  provider_sha256.process_challenge(challenge_sha256);

  // SHA-256-sess 사용
  DigestAuthProvider provider_sha256_sess(creds);
  AuthChallenge challenge_sha256_sess;
  challenge_sha256_sess.scheme = AuthScheme::kDigest;
  challenge_sha256_sess.realm = "test";
  challenge_sha256_sess.nonce = "nonce123";
  challenge_sha256_sess.algorithm = "SHA-256-sess";
  challenge_sha256_sess.qop = "auth";
  provider_sha256_sess.process_challenge(challenge_sha256_sess);

  AuthContext context{"GET", "/resource"};

  auto result_sha256 = provider_sha256.generate_authorization_header(context);
  auto result_sha256_sess
    = provider_sha256_sess.generate_authorization_header(context);

  ASSERT_TRUE(result_sha256.has_value());
  ASSERT_TRUE(result_sha256_sess.has_value());

  // 두 응답이 달라야 함 (HA1 계산 방식이 다르므로)
  EXPECT_NE(*result_sha256, *result_sha256_sess);
}

// MD5-sess 알고리즘도 지원 확인
TEST(DigestAuthTest, Md5SessAlgorithm)
{
  Credentials creds{"user", "pass"};
  DigestAuthProvider provider(creds);

  AuthChallenge challenge;
  challenge.scheme = AuthScheme::kDigest;
  challenge.realm = "test";
  challenge.nonce = "xyz789";
  challenge.algorithm = "MD5-sess";
  challenge.qop = "auth";

  auto error = provider.process_challenge(challenge);
  EXPECT_FALSE(error);

  AuthContext context{"GET", "/data"};
  auto result = provider.generate_authorization_header(context);

  ASSERT_TRUE(result.has_value());
  EXPECT_NE(result->find("algorithm=MD5-sess"), std::string::npos);
}