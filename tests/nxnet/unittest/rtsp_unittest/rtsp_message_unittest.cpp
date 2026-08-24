// 파일: rtsp_message_unittest.cpp
// 생성일: 2026-02-23
// 설명: RTSP 메시지 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/rtsp/rtsp_message.h"

// ============================================================================
// RtspRequest 직렬화 테스트
// ============================================================================

TEST(RtspMessageTest, SerializeOptionsRequest)
{
  nx::net::RtspRequest request;
  request.method = nx::net::RtspMethod::kOptions;
  request.uri = "rtsp://192.168.0.168:554/media/video1";
  request.cseq = 1;
  request.headers["User-Agent"] = "HiVe2 RTSP Client";

  auto serialized = request.serialize();

  EXPECT_NE(
    serialized.find("OPTIONS rtsp://192.168.0.168:554/media/video1 RTSP/1.0"),
    std::string::npos);
  EXPECT_NE(serialized.find("CSeq: 1"), std::string::npos);
  EXPECT_NE(serialized.find("User-Agent: HiVe2 RTSP Client"), std::string::npos);
  EXPECT_NE(serialized.find("\r\n\r\n"), std::string::npos);
}

TEST(RtspMessageTest, SerializeDescribeRequest)
{
  nx::net::RtspRequest request;
  request.method = nx::net::RtspMethod::kDescribe;
  request.uri = "rtsp://192.168.0.168:554/media/video1";
  request.cseq = 2;
  request.headers["Accept"] = "application/sdp";

  auto serialized = request.serialize();

  EXPECT_NE(serialized.find("DESCRIBE"), std::string::npos);
  EXPECT_NE(serialized.find("Accept: application/sdp"), std::string::npos);
  EXPECT_NE(serialized.find("CSeq: 2"), std::string::npos);
}

TEST(RtspMessageTest, SerializeSetupRequestTcp)
{
  nx::net::RtspRequest request;
  request.method = nx::net::RtspMethod::kSetup;
  request.uri = "rtsp://192.168.0.168:554/media/video1/trackID=0";
  request.cseq = 3;
  request.headers["Transport"] = "RTP/AVP/TCP;unicast;interleaved=0-1";

  auto serialized = request.serialize();

  EXPECT_NE(serialized.find("SETUP"), std::string::npos);
  EXPECT_NE(
    serialized.find("Transport: RTP/AVP/TCP;unicast;interleaved=0-1"),
    std::string::npos);
}

TEST(RtspMessageTest, SerializePlayRequest)
{
  nx::net::RtspRequest request;
  request.method = nx::net::RtspMethod::kPlay;
  request.uri = "rtsp://192.168.0.168:554/media/video1";
  request.cseq = 4;
  request.headers["Session"] = "12345678";
  request.headers["Range"] = "npt=0.0-";

  auto serialized = request.serialize();

  EXPECT_NE(serialized.find("PLAY"), std::string::npos);
  EXPECT_NE(serialized.find("Session: 12345678"), std::string::npos);
  EXPECT_NE(serialized.find("Range: npt=0.0-"), std::string::npos);
}

TEST(RtspMessageTest, SerializeRequestWithBody)
{
  nx::net::RtspRequest request;
  request.method = nx::net::RtspMethod::kSetParameter;
  request.uri = "rtsp://192.168.0.168:554/media/video1";
  request.cseq = 5;
  request.body = "parameter=value";

  auto serialized = request.serialize();

  EXPECT_NE(serialized.find("Content-Length: 15"), std::string::npos);
  EXPECT_NE(serialized.find("parameter=value"), std::string::npos);
}

// ============================================================================
// RtspResponse 테스트
// ============================================================================

TEST(RtspMessageTest, ResponseIsSuccess)
{
  nx::net::RtspResponse response;
  response.status_code = 200;
  EXPECT_TRUE(response.is_success());

  response.status_code = 401;
  EXPECT_FALSE(response.is_success());

  response.status_code = 500;
  EXPECT_FALSE(response.is_success());
}

TEST(RtspMessageTest, FindHeaderCaseInsensitive)
{
  nx::net::RtspResponse response;
  response.headers["Content-Type"] = "application/sdp";
  response.headers["Session"] = "12345";

  EXPECT_EQ(response.find_header("content-type"), "application/sdp");
  EXPECT_EQ(response.find_header("CONTENT-TYPE"), "application/sdp");
  EXPECT_EQ(response.find_header("Content-Type"), "application/sdp");
  EXPECT_EQ(response.find_header("session"), "12345");
  EXPECT_EQ(response.find_header("NonExistent"), "");
}

// ============================================================================
// RtspMethod 변환 테스트
// ============================================================================

TEST(RtspMessageTest, MethodToString)
{
  EXPECT_STREQ(
    nx::net::rtsp_method_to_string(nx::net::RtspMethod::kOptions),
    "OPTIONS");
  EXPECT_STREQ(
    nx::net::rtsp_method_to_string(nx::net::RtspMethod::kDescribe),
    "DESCRIBE");
  EXPECT_STREQ(nx::net::rtsp_method_to_string(nx::net::RtspMethod::kSetup), "SETUP");
  EXPECT_STREQ(nx::net::rtsp_method_to_string(nx::net::RtspMethod::kPlay), "PLAY");
  EXPECT_STREQ(nx::net::rtsp_method_to_string(nx::net::RtspMethod::kPause), "PAUSE");
  EXPECT_STREQ(
    nx::net::rtsp_method_to_string(nx::net::RtspMethod::kTeardown),
    "TEARDOWN");
  EXPECT_STREQ(
    nx::net::rtsp_method_to_string(nx::net::RtspMethod::kGetParameter),
    "GET_PARAMETER");
  EXPECT_STREQ(
    nx::net::rtsp_method_to_string(nx::net::RtspMethod::kSetParameter),
    "SET_PARAMETER");
}
