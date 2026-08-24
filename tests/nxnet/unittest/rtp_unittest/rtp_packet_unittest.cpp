// 파일: rtp_packet_unittest.cpp
// 생성일: 2026-02-23
// 설명: RTP 패킷 파싱 단위 테스트

#include <gtest/gtest.h>
#include "nxnet/rtp/rtp_packet.h"

// ============================================================================
// RTP 패킷 파싱 테스트
// ============================================================================

TEST(RtpPacketTest, ParseMinimalPacket)
{
  // RTP 패킷: V=2, P=0, X=0, CC=0, M=1, PT=96, Seq=1000, TS=12345, SSRC=1
  std::vector<uint8_t> data = {
    0x80, // V=2, P=0, X=0, CC=0
    0xE0, // M=1, PT=96
    0x03,
    0xE8, // Seq=1000
    0x00,
    0x00,
    0x30,
    0x39, // TS=12345
    0x00,
    0x00,
    0x00,
    0x01, // SSRC=1
    // 페이로드
    0x65,
    0x01,
    0x02,
    0x03};

  auto result = nx::rtp::RtpPacket::parse(data);
  ASSERT_TRUE(result.has_value());

  const auto& header = result->header();
  EXPECT_EQ(header.version, 2);
  EXPECT_FALSE(header.padding);
  EXPECT_FALSE(header.extension);
  EXPECT_EQ(header.csrc_count, 0);
  EXPECT_TRUE(header.marker);
  EXPECT_EQ(header.payload_type, 96);
  EXPECT_EQ(header.sequence_number, 1000);
  EXPECT_EQ(header.timestamp, 12345);
  EXPECT_EQ(header.ssrc, 1);

  auto payload = result->payload();
  ASSERT_EQ(payload.size(), 4);
  EXPECT_EQ(payload[0], 0x65);
  EXPECT_EQ(payload[1], 0x01);
  EXPECT_EQ(payload[2], 0x02);
  EXPECT_EQ(payload[3], 0x03);
}

TEST(RtpPacketTest, ParsePacketWithCsrc)
{
  std::vector<uint8_t> data = {
    0x82, // V=2, P=0, X=0, CC=2
    0x60, // M=0, PT=96
    0x00,
    0x01, // Seq=1
    0x00,
    0x00,
    0x00,
    0x0A, // TS=10
    0x00,
    0x00,
    0x00,
    0x01, // SSRC=1
    0x00,
    0x00,
    0x00,
    0x02, // CSRC[0]=2
    0x00,
    0x00,
    0x00,
    0x03, // CSRC[1]=3
    // 페이로드
    0xAA,
    0xBB};

  auto result = nx::rtp::RtpPacket::parse(data);
  ASSERT_TRUE(result.has_value());

  const auto& header = result->header();
  EXPECT_EQ(header.csrc_count, 2);
  ASSERT_EQ(header.csrc_list.size(), 2);
  EXPECT_EQ(header.csrc_list[0], 2);
  EXPECT_EQ(header.csrc_list[1], 3);

  EXPECT_EQ(result->payload().size(), 2);
}

TEST(RtpPacketTest, ParsePacketWithExtension)
{
  std::vector<uint8_t> data = {
    0x90, // V=2, P=0, X=1, CC=0
    0x60, // M=0, PT=96
    0x00,
    0x01, // Seq=1
    0x00,
    0x00,
    0x00,
    0x0A, // TS=10
    0x00,
    0x00,
    0x00,
    0x01, // SSRC=1
    // 확장 헤더
    0xBE,
    0xDE, // Profile=0xBEDE
    0x00,
    0x01, // Length=1 (32비트 워드)
    0x11,
    0x22,
    0x33,
    0x44, // 확장 데이터
    // 페이로드
    0xAA};

  auto result = nx::rtp::RtpPacket::parse(data);
  ASSERT_TRUE(result.has_value());

  const auto& header = result->header();
  EXPECT_TRUE(header.extension);
  ASSERT_TRUE(header.ext_header.has_value());
  EXPECT_EQ(header.ext_header->profile, 0xBEDE);
  EXPECT_EQ(header.ext_header->data.size(), 4);

  EXPECT_EQ(result->payload().size(), 1);
  EXPECT_EQ(result->payload()[0], 0xAA);
}

TEST(RtpPacketTest, ParsePacketWithPadding)
{
  std::vector<uint8_t> data = {
    0xA0, // V=2, P=1, X=0, CC=0
    0x60, // M=0, PT=96
    0x00,
    0x01, // Seq=1
    0x00,
    0x00,
    0x00,
    0x0A, // TS=10
    0x00,
    0x00,
    0x00,
    0x01, // SSRC=1
    // 페이로드
    0xAA,
    0xBB,
    // 패딩 (2바이트)
    0x00,
    0x02};

  auto result = nx::rtp::RtpPacket::parse(data);
  ASSERT_TRUE(result.has_value());

  const auto& header = result->header();
  EXPECT_TRUE(header.padding);

  // 패딩 제외한 페이로드
  EXPECT_EQ(result->payload().size(), 2);
  EXPECT_EQ(result->payload()[0], 0xAA);
  EXPECT_EQ(result->payload()[1], 0xBB);
}

TEST(RtpPacketTest, ParseTooShort)
{
  std::vector<uint8_t> data = {0x80, 0x60};

  auto result = nx::rtp::RtpPacket::parse(data);
  EXPECT_FALSE(result.has_value());
}

TEST(RtpPacketTest, ParseInvalidVersion)
{
  std::vector<uint8_t> data = {
    0x00, // V=0 (잘못된 버전)
    0x60,
    0x00,
    0x01,
    0x00,
    0x00,
    0x00,
    0x0A,
    0x00,
    0x00,
    0x00,
    0x01,
  };

  auto result = nx::rtp::RtpPacket::parse(data);
  EXPECT_FALSE(result.has_value());
}

TEST(RtpPacketTest, ParseEmptyPayload)
{
  std::vector<uint8_t> data = {
    0x80, // V=2, P=0, X=0, CC=0
    0x60, // M=0, PT=96
    0x00,
    0x01, // Seq=1
    0x00,
    0x00,
    0x00,
    0x0A, // TS=10
    0x00,
    0x00,
    0x00,
    0x01, // SSRC=1
  };

  auto result = nx::rtp::RtpPacket::parse(data);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->payload().size(), 0);
}
