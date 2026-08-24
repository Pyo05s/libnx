// 파일: uri_util_unittest.cpp
// 생성일: 2026-02-19
// 설명: URI 파싱 및 조작 유틸리티 단위 테스트

#include <nxcore/util/uri_util.h>

#include <gtest/gtest.h>

namespace nx {

// ============================================================================
// URI 파싱 테스트
// ============================================================================

TEST(UriUtilTest, ParseUri_RtspUrl)
{
  // RTSP URI 파싱 후 각 구성 요소 검증
  auto result = parse_uri("rtsp://192.168.0.168:554/stream1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->scheme, "rtsp");
  EXPECT_EQ(result->host, "192.168.0.168");
  EXPECT_EQ(result->port, 554);
  EXPECT_EQ(result->path, "/stream1");
}

TEST(UriUtilTest, ParseUri_HttpWithPath)
{
  // HTTP URI + 경로 파싱 검증
  auto result = parse_uri("http://192.168.0.168:80/onvif/device_service");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->scheme, "http");
  EXPECT_EQ(result->host, "192.168.0.168");
  EXPECT_EQ(result->port, 80);
  EXPECT_EQ(result->path, "/onvif/device_service");
}

TEST(UriUtilTest, ParseUri_EmptyString)
{
  // 빈 문자열 입력 시 nullopt 또는 빈 결과 반환 검증
  auto result = parse_uri("");

  // 빈 문자열은 파싱 실패이므로 nullopt 반환
  EXPECT_FALSE(result.has_value());
}

TEST(UriUtilTest, ParseUri_WithUsernameAndPassword)
{
  // username:password@host 형식 파싱 검증
  auto result = parse_uri("rtsp://admin:pass123@192.168.0.168:554/stream1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->scheme, "rtsp");
  EXPECT_EQ(result->username, "admin");
  EXPECT_EQ(result->password, "pass123");
  EXPECT_EQ(result->host, "192.168.0.168");
  EXPECT_EQ(result->port, 554);
  EXPECT_EQ(result->path, "/stream1");
}

TEST(UriUtilTest, ParseUri_WithUsernameOnly)
{
  // username만 있고 password 없는 형식 파싱 검증
  auto result = parse_uri("rtsp://admin@192.168.0.168:554/stream1");

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->username, "admin");
  EXPECT_TRUE(result->password.empty());
  EXPECT_EQ(result->host, "192.168.0.168");
}

TEST(UriUtilTest, BuildUri_WithUserinfo)
{
  // userinfo 포함된 UriComponents로 URI 문자열 생성 검증
  UriComponents components;
  components.scheme = "rtsp";
  components.username = "admin";
  components.password = "pass123";
  components.host = "192.168.0.168";
  components.port = 8554;
  components.path = "/stream1";

  EXPECT_EQ(build_uri(components), "rtsp://admin:pass123@192.168.0.168:8554/stream1");
}

TEST(UriUtilTest, ParseUri_UserinfoPreservedOnRoundTrip)
{
  // 파싱 후 재조립 시 userinfo 포함 원본 URI 복원 검증
  const std::string original = "rtsp://admin:pass123@192.168.0.168:8554/stream1";
  auto result = parse_uri(original);

  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(build_uri(*result), original);
}

// ============================================================================
// URI 조작 테스트
// ============================================================================

TEST(UriUtilTest, ReplaceUriAuthority_ReplaceHostAndPort)
{
  // 호스트 + 포트 모두 교체 검증
  std::string result = replace_uri_authority(
    "rtsp://192.168.0.100:554/stream1",
    "192.168.0.168",
    8554);

  EXPECT_EQ(result, "rtsp://192.168.0.168:8554/stream1");
}

TEST(UriUtilTest, ReplaceUriHost_KeepPort)
{
  // 포트는 유지하고 호스트만 교체 검증
  // 비기본 포트(8554)를 사용하여 포트 생략 여부와 무관하게 검증
  std::string result
    = replace_uri_host("rtsp://192.168.0.100:8554/stream1", "10.0.0.1");

  // 포트 8554는 유지되어야 함
  EXPECT_EQ(result, "rtsp://10.0.0.1:8554/stream1");
}

TEST(UriUtilTest, ReplaceUriPort_KeepHost)
{
  // 호스트는 유지하고 포트만 교체 검증
  std::string result = replace_uri_port("rtsp://192.168.0.168:554/stream1", 8554);

  // 호스트 192.168.0.168은 유지되어야 함
  EXPECT_EQ(result, "rtsp://192.168.0.168:8554/stream1");
}

// ============================================================================
// URL 인코딩/디코딩 테스트
// ============================================================================

TEST(UriUtilTest, UrlEncode_SpecialChars)
{
  // 공백 및 특수문자 인코딩 검증
  // 공백 → %20, @ → %40 등
  std::string result = url_encode("hello world");
  EXPECT_EQ(result, "hello%20world");

  // 특수문자 인코딩
  std::string result2 = url_encode("user@example.com");
  EXPECT_FALSE(result2.find("%40") == std::string::npos);
}

TEST(UriUtilTest, UrlDecode_EncodedString)
{
  // 인코딩된 문자열 복원 검증
  std::string result = url_decode("hello%20world");
  EXPECT_EQ(result, "hello world");

  // %40 → @
  std::string result2 = url_decode("user%40example.com");
  EXPECT_EQ(result2, "user@example.com");
}

} // namespace nx
