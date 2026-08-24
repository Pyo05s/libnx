// 파일: sdp_parser_unittest.cpp
// 생성일: 2026-02-23
// 설명: SDP 파서 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/sdp/sdp_parser.h"

// ============================================================================
// 기본 SDP 파싱 테스트
// ============================================================================

TEST(SdpParserTest, ParseBasicSession)
{
  const char* sdp = "v=0\r\n"
                    "o=- 123 456 IN IP4 192.168.1.1\r\n"
                    "s=Test Session\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "t=0 0\r\n"
                    "m=video 0 RTP/AVP 96\r\n"
                    "a=rtpmap:96 H264/90000\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  ASSERT_TRUE(result.has_value());

  const auto& session = *result;
  EXPECT_EQ(session.session_name(), "Test Session");
  EXPECT_EQ(session.connection_address(), "0.0.0.0");
  EXPECT_EQ(session.media_descriptions().size(), 1);

  const auto& video = session.media_descriptions()[0];
  EXPECT_EQ(video.type, nx::sdp::SdpMediaType::kVideo);
  EXPECT_EQ(video.protocol, "RTP/AVP");
  EXPECT_EQ(video.rtpmap.at(96), "H264/90000");
}

TEST(SdpParserTest, ParseVideoAndAudio)
{
  const char* sdp = "v=0\r\n"
                    "o=- 1234567890 1234567890 IN IP4 192.168.1.100\r\n"
                    "s=IP Camera\r\n"
                    "c=IN IP4 0.0.0.0\r\n"
                    "t=0 0\r\n"
                    "a=control:*\r\n"
                    "m=video 0 RTP/AVP 96\r\n"
                    "a=rtpmap:96 H264/90000\r\n"
                    "a=fmtp:96 packetization-mode=1;profile-level-id=42e01f\r\n"
                    "a=control:trackID=0\r\n"
                    "a=framerate:30.000000\r\n"
                    "m=audio 0 RTP/AVP 97\r\n"
                    "a=rtpmap:97 MPEG4-GENERIC/16000/1\r\n"
                    "a=fmtp:97 streamtype=5;profile-level-id=15;mode=AAC-hbr\r\n"
                    "a=control:trackID=1\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  ASSERT_TRUE(result.has_value());

  const auto& session = *result;
  EXPECT_EQ(session.session_name(), "IP Camera");
  EXPECT_EQ(session.base_url(), "*");
  EXPECT_EQ(session.media_descriptions().size(), 2);
  EXPECT_TRUE(session.has_video());
  EXPECT_TRUE(session.has_audio());

  // 비디오 트랙 확인
  const auto* video = session.find_media(nx::sdp::SdpMediaType::kVideo);
  ASSERT_NE(video, nullptr);
  EXPECT_EQ(video->control_url, "trackID=0");
  EXPECT_DOUBLE_EQ(*video->framerate, 30.0);
  EXPECT_EQ(video->fmtp.at(96), "packetization-mode=1;profile-level-id=42e01f");

  // 오디오 트랙 확인
  const auto* audio = session.find_media(nx::sdp::SdpMediaType::kAudio);
  ASSERT_NE(audio, nullptr);
  EXPECT_EQ(audio->control_url, "trackID=1");
  EXPECT_EQ(audio->rtpmap.at(97), "MPEG4-GENERIC/16000/1");
}

TEST(SdpParserTest, ParseEmptyInput)
{
  auto result = nx::sdp::SdpParser::parse("");
  EXPECT_FALSE(result.has_value());
}

TEST(SdpParserTest, ParseMissingVersion)
{
  const char* sdp = "o=- 123 456 IN IP4 192.168.1.1\r\n"
                    "s=Test\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  EXPECT_FALSE(result.has_value());
}

TEST(SdpParserTest, ParseUnsupportedVersion)
{
  const char* sdp = "v=1\r\n"
                    "o=- 123 456 IN IP4 192.168.1.1\r\n"
                    "s=Test\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  EXPECT_FALSE(result.has_value());
}

TEST(SdpParserTest, ParseOrigin)
{
  const char* sdp = "v=0\r\n"
                    "o=admin 12345 67890 IN IP4 192.168.0.100\r\n"
                    "s=Camera\r\n"
                    "t=0 0\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  ASSERT_TRUE(result.has_value());

  const auto& origin = result->origin();
  EXPECT_EQ(origin.username, "admin");
  EXPECT_EQ(origin.session_id, 12345);
  EXPECT_EQ(origin.session_version, 67890);
  EXPECT_EQ(origin.address, "192.168.0.100");
}

TEST(SdpParserTest, ParseMediaLevelConnection)
{
  const char* sdp = "v=0\r\n"
                    "o=- 123 456 IN IP4 192.168.1.1\r\n"
                    "s=Test\r\n"
                    "t=0 0\r\n"
                    "m=video 0 RTP/AVP 96\r\n"
                    "c=IN IP4 192.168.1.50\r\n"
                    "a=rtpmap:96 H264/90000\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  ASSERT_TRUE(result.has_value());

  const auto& video = result->media_descriptions()[0];
  EXPECT_EQ(video.connection_address, "192.168.1.50");
}

TEST(SdpParserTest, ParseMultipleFormats)
{
  const char* sdp = "v=0\r\n"
                    "o=- 123 456 IN IP4 192.168.1.1\r\n"
                    "s=Test\r\n"
                    "t=0 0\r\n"
                    "m=video 0 RTP/AVP 96 97\r\n"
                    "a=rtpmap:96 H264/90000\r\n"
                    "a=rtpmap:97 H265/90000\r\n";

  auto result = nx::sdp::SdpParser::parse(sdp);
  ASSERT_TRUE(result.has_value());

  const auto& video = result->media_descriptions()[0];
  ASSERT_EQ(video.formats.size(), 2);
  EXPECT_EQ(video.formats[0], 96);
  EXPECT_EQ(video.formats[1], 97);
}
