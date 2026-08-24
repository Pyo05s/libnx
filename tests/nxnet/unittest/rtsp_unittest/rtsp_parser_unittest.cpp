// 파일: rtsp_parser_unittest.cpp
// 생성일: 2026-02-23
// 설명: RTSP 파서 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/rtsp/rtsp_parser.h"

// ============================================================================
// RTSP 응답 파싱 테스트
// ============================================================================

TEST(RtspParserTest, ParseOptionsResponse)
{
  const char* response_data
    = "RTSP/1.0 200 OK\r\n"
      "CSeq: 1\r\n"
      "Public: OPTIONS, DESCRIBE, SETUP, PLAY, PAUSE, TEARDOWN\r\n"
      "\r\n";

  auto result = nx::net::RtspParser::parse_response(response_data);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->version, "RTSP/1.0");
  EXPECT_EQ(result->status_code, 200);
  EXPECT_EQ(result->reason_phrase, "OK");
  EXPECT_EQ(result->cseq, 1);
  EXPECT_TRUE(result->is_success());

  auto public_header = result->find_header("Public");
  EXPECT_FALSE(public_header.empty());
}

TEST(RtspParserTest, ParseDescribeResponse)
{
  std::string sdp_body = "v=0\r\n"
                         "o=- 123 456 IN IP4 192.168.1.1\r\n"
                         "s=Camera\r\n"
                         "t=0 0\r\n"
                         "m=video 0 RTP/AVP 96\r\n"
                         "a=rtpmap:96 H264/90000\r\n";

  std::string response_str = "RTSP/1.0 200 OK\r\n"
                             "CSeq: 2\r\n"
                             "Content-Type: application/sdp\r\n"
                             "Content-Length: "
                             + std::to_string(sdp_body.size())
                             + "\r\n"
                               "\r\n"
                             + sdp_body;

  auto result = nx::net::RtspParser::parse_response(response_str);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->status_code, 200);
  EXPECT_EQ(result->cseq, 2);
  EXPECT_EQ(result->body, sdp_body);
}

TEST(RtspParserTest, ParseUnauthorizedResponse)
{
  const char* response_data
    = "RTSP/1.0 401 Unauthorized\r\n"
      "CSeq: 1\r\n"
      "WWW-Authenticate: Digest realm=\"IP Camera\", nonce=\"abc123\", "
      "qop=\"auth\"\r\n"
      "\r\n";

  auto result = nx::net::RtspParser::parse_response(response_data);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->status_code, 401);
  EXPECT_FALSE(result->is_success());

  auto www_auth = result->find_header("WWW-Authenticate");
  EXPECT_FALSE(www_auth.empty());
  EXPECT_NE(www_auth.find("Digest"), std::string::npos);
}

TEST(RtspParserTest, ParseSetupResponse)
{
  const char* response_data = "RTSP/1.0 200 OK\r\n"
                              "CSeq: 3\r\n"
                              "Session: 12345678;timeout=60\r\n"
                              "Transport: RTP/AVP/TCP;unicast;interleaved=0-1\r\n"
                              "\r\n";

  auto result = nx::net::RtspParser::parse_response(response_data);
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->status_code, 200);
  EXPECT_EQ(result->cseq, 3);

  auto session = result->find_header("Session");
  EXPECT_FALSE(session.empty());
  EXPECT_NE(session.find("12345678"), std::string::npos);
}

TEST(RtspParserTest, ParseEmptyResponse)
{
  auto result = nx::net::RtspParser::parse_response("");
  EXPECT_FALSE(result.has_value());
}

TEST(RtspParserTest, ParseInvalidResponse)
{
  auto result = nx::net::RtspParser::parse_response("not a valid response");
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Transport 헤더 파싱 테스트
// ============================================================================

TEST(RtspParserTest, ParseTransportTcpInterleaved)
{
  auto result
    = nx::net::RtspParser::parse_transport("RTP/AVP/TCP;unicast;interleaved=0-1");
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->transport, nx::net::RtspTransport::kRtpTcp);
  EXPECT_TRUE(result->unicast);
  ASSERT_TRUE(result->interleaved_rtp.has_value());
  ASSERT_TRUE(result->interleaved_rtcp.has_value());
  EXPECT_EQ(*result->interleaved_rtp, 0);
  EXPECT_EQ(*result->interleaved_rtcp, 1);
}

TEST(RtspParserTest, ParseTransportUdp)
{
  auto result = nx::net::RtspParser::parse_transport(
    "RTP/AVP;unicast;client_port=50000-50001;server_port=60000-60001");
  ASSERT_TRUE(result.has_value());

  EXPECT_EQ(result->transport, nx::net::RtspTransport::kRtpUdp);
  EXPECT_TRUE(result->unicast);
  ASSERT_TRUE(result->client_rtp_port.has_value());
  ASSERT_TRUE(result->client_rtcp_port.has_value());
  EXPECT_EQ(*result->client_rtp_port, 50000);
  EXPECT_EQ(*result->client_rtcp_port, 50001);
  EXPECT_EQ(result->rtp_port, 60000);
  EXPECT_EQ(result->rtcp_port, 60001);
}

TEST(RtspParserTest, ParseTransportWithSsrc)
{
  auto result = nx::net::RtspParser::parse_transport(
    "RTP/AVP/TCP;unicast;interleaved=0-1;ssrc=ABCDEF01");
  ASSERT_TRUE(result.has_value());

  ASSERT_TRUE(result->ssrc.has_value());
  EXPECT_EQ(*result->ssrc, "ABCDEF01");
}

TEST(RtspParserTest, ParseTransportEmpty)
{
  auto result = nx::net::RtspParser::parse_transport("");
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// 응답 완전성 검사 테스트
// ============================================================================

TEST(RtspParserTest, FindResponseEndComplete)
{
  std::string data = "RTSP/1.0 200 OK\r\n"
                     "CSeq: 1\r\n"
                     "\r\n";

  auto end = nx::net::RtspParser::find_response_end(data);
  EXPECT_EQ(end, data.size());
}

TEST(RtspParserTest, FindResponseEndWithBody)
{
  std::string body = "test body data";
  std::string data = "RTSP/1.0 200 OK\r\n"
                     "CSeq: 1\r\n"
                     "Content-Length: "
                     + std::to_string(body.size())
                     + "\r\n"
                       "\r\n"
                     + body;

  auto end = nx::net::RtspParser::find_response_end(data);
  EXPECT_EQ(end, data.size());
}

TEST(RtspParserTest, FindResponseEndIncomplete)
{
  std::string data = "RTSP/1.0 200 OK\r\n"
                     "CSeq: 1\r\n";

  auto end = nx::net::RtspParser::find_response_end(data);
  EXPECT_EQ(end, 0);
}

TEST(RtspParserTest, FindResponseEndIncompleteBody)
{
  std::string data = "RTSP/1.0 200 OK\r\n"
                     "CSeq: 1\r\n"
                     "Content-Length: 100\r\n"
                     "\r\n"
                     "partial";

  auto end = nx::net::RtspParser::find_response_end(data);
  EXPECT_EQ(end, 0);
}
