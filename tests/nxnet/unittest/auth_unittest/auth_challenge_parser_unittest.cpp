// 파일: auth_challenge_parser_unittest.cpp
// 생성일: 2026-02-10
// 설명: AuthChallengeParser 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/auth/auth_challenge_parser.h"
#include "nxnet/auth/auth_types.h"
#include "nxnet/auth/auth_error.h"

using namespace nx::net::auth;

// Basic 인증 Challenge 파싱 테스트
TEST(AuthChallengeParserTest, ParseBasicChallenge)
{
  std::string header = "Basic realm=\"example\"";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_TRUE(result.has_value());

  const auto& challenge = *result;
  EXPECT_EQ(challenge.scheme, AuthScheme::kBasic);
  EXPECT_EQ(challenge.realm, "example");
}

// Digest 인증 Challenge 파싱 테스트
TEST(AuthChallengeParserTest, ParseDigestChallenge)
{
  std::string header = "Digest realm=\"testrealm@host.com\", "
                       "nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", "
                       "qop=\"auth\", "
                       "algorithm=\"MD5\"";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_TRUE(result.has_value());

  const auto& challenge = *result;
  EXPECT_EQ(challenge.scheme, AuthScheme::kDigest);
  EXPECT_EQ(challenge.realm, "testrealm@host.com");
  EXPECT_TRUE(challenge.nonce.has_value());
  EXPECT_EQ(*challenge.nonce, "dcd98b7102dd2f0e8b11d0f600bfb0c093");
  EXPECT_TRUE(challenge.qop.has_value());
  EXPECT_EQ(*challenge.qop, "auth");
  EXPECT_TRUE(challenge.algorithm.has_value());
  EXPECT_EQ(*challenge.algorithm, "MD5");
}

// Bearer Token Challenge 파싱 테스트
TEST(AuthChallengeParserTest, ParseBearerChallenge)
{
  std::string header = "Bearer realm=\"example\"";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_TRUE(result.has_value());

  const auto& challenge = *result;
  EXPECT_EQ(challenge.scheme, AuthScheme::kBearer);
  EXPECT_EQ(challenge.realm, "example");
}

// 빈 헤더 테스트
TEST(AuthChallengeParserTest, ParseEmptyHeader)
{
  std::string header = "";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kNoChallenge));
}

// 잘못된 형식 테스트
TEST(AuthChallengeParserTest, ParseInvalidFormat)
{
  std::string header = "InvalidFormat";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kInvalidChallenge));
}

// 지원하지 않는 스킴 테스트
TEST(AuthChallengeParserTest, ParseUnsupportedScheme)
{
  std::string header = "Unknown realm=\"example\"";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(AuthError::kUnsupportedScheme));
}

// Digest with opaque 테스트
TEST(AuthChallengeParserTest, ParseDigestWithOpaque)
{
  std::string header = "Digest realm=\"test\", "
                       "nonce=\"abc123\", "
                       "opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_TRUE(result.has_value());

  const auto& challenge = *result;
  EXPECT_EQ(challenge.scheme, AuthScheme::kDigest);
  EXPECT_TRUE(challenge.opaque.has_value());
  EXPECT_EQ(*challenge.opaque, "5ccc069c403ebaf9f0171e9517f40e41");
}

// stale 파라미터 테스트
TEST(AuthChallengeParserTest, ParseDigestWithStale)
{
  std::string header = "Digest realm=\"test\", "
                       "nonce=\"abc123\", "
                       "stale=true";

  auto result = AuthChallengeParser::parse(header);
  ASSERT_TRUE(result.has_value());

  const auto& challenge = *result;
  EXPECT_TRUE(challenge.stale.has_value());
  EXPECT_TRUE(*challenge.stale);
}