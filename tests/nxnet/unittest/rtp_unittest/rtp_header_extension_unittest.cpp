// 파일: rtp_header_extension_unittest.cpp
// 생성일: 2026-03-30
// 설명: RFC 5285 RTP Header Extension 빌더/파서 단위 테스트

#include <gtest/gtest.h>

#include <nxnet/rtp/rtp_header_extension.h>
#include <nxnet/rtp/rtp_h264_packetizer.h>
#include <nxnet/rtp/rtp_g711_packetizer.h>
#include <nxnet/rtp/rtp_frame_buffer.h>
#include <nxnet/rtp/rtp_packet.h>

#include <cstdint>
#include <vector>

// ============================================================================
// RtpHeaderExtensionBuilder 테스트
// ============================================================================

TEST(RtpHeaderExtensionBuilderTest, EmptyBuilder_SerializesNothing)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;

  EXPECT_TRUE(builder.empty());
  EXPECT_EQ(builder.element_count(), 0u);
  EXPECT_EQ(builder.serialized_size(), 0u);

  std::vector<uint8_t> buf;
  builder.serialize(buf);
  EXPECT_TRUE(buf.empty());
}

TEST(RtpHeaderExtensionBuilderTest, AddNtp64_CorrectSize)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;
  builder.add_ntp64(1, 0x1234567890ABCDEFULL);

  EXPECT_FALSE(builder.empty());
  EXPECT_EQ(builder.element_count(), 1u);

  // 4 (블록 헤더) + 1 (요소 헤더) + 8 (NTP-64) + 3 (패딩) = 16
  EXPECT_EQ(builder.serialized_size(), 16u);
}

TEST(RtpHeaderExtensionBuilderTest, AddNtp64_CorrectSerialization)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;
  uint64_t ntp = 0xAABBCCDD11223344ULL;
  builder.add_ntp64(1, ntp);

  std::vector<uint8_t> buf;
  builder.serialize(buf);

  ASSERT_EQ(buf.size(), 16u);

  // 프로필 0xBEDE
  EXPECT_EQ(buf[0], 0xBE);
  EXPECT_EQ(buf[1], 0xDE);

  // Length = 3 words (12 bytes of content area: 1+8+3padding = 12 → 3 words)
  uint16_t length = (buf[2] << 8) | buf[3];
  EXPECT_EQ(length, 3u);

  // 요소 헤더: ID=1, L=7 (데이터 8바이트 = L+1)
  EXPECT_EQ(buf[4], 0x17u);

  // NTP-64 데이터 (Big-Endian)
  EXPECT_EQ(buf[5], 0xAA);
  EXPECT_EQ(buf[6], 0xBB);
  EXPECT_EQ(buf[7], 0xCC);
  EXPECT_EQ(buf[8], 0xDD);
  EXPECT_EQ(buf[9], 0x11);
  EXPECT_EQ(buf[10], 0x22);
  EXPECT_EQ(buf[11], 0x33);
  EXPECT_EQ(buf[12], 0x44);

  // 패딩 3바이트
  EXPECT_EQ(buf[13], 0x00);
  EXPECT_EQ(buf[14], 0x00);
  EXPECT_EQ(buf[15], 0x00);
}

TEST(RtpHeaderExtensionBuilderTest, MultipleElements)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;

  // ID 1: 4 bytes
  std::vector<uint8_t> data1 = {0x01, 0x02, 0x03, 0x04};
  builder.add(1, data1);

  // ID 2: 2 bytes
  std::vector<uint8_t> data2 = {0xAA, 0xBB};
  builder.add(2, data2);

  EXPECT_EQ(builder.element_count(), 2u);

  // 컨텐츠: (1+4) + (1+2) = 8 bytes → 패딩 0 → 4 + 8 = 12
  EXPECT_EQ(builder.serialized_size(), 12u);
}

TEST(RtpHeaderExtensionBuilderTest, InvalidId_Ignored)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;
  std::vector<uint8_t> data = {0x01};

  builder.add(0, data);  // ID 0 → 무시
  builder.add(15, data); // ID 15 → 무시
  EXPECT_TRUE(builder.empty());
}

TEST(RtpHeaderExtensionBuilderTest, EmptyData_Ignored)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;
  builder.add(1, std::span<const uint8_t>{});
  EXPECT_TRUE(builder.empty());
}

// ============================================================================
// parse_one_byte_extension 테스트
// ============================================================================

TEST(RtpHeaderExtensionParserTest, ParseNtp64)
{
  // ID=1, L=7 (8 bytes), data = NTP-64
  std::vector<uint8_t> ext_data = {
    0x17, // ID=1, L=7
    0xAA,
    0xBB,
    0xCC,
    0xDD,
    0x11,
    0x22,
    0x33,
    0x44,
    0x00,
    0x00,
    0x00 // 패딩
  };

  auto elements = nx::rtp::parse_one_byte_extension(ext_data);
  ASSERT_EQ(elements.size(), 1u);
  EXPECT_EQ(elements[0].id, 1);
  ASSERT_EQ(elements[0].data.size(), 8u);

  auto ntp = nx::rtp::extract_ntp64(elements, 1);
  ASSERT_TRUE(ntp.has_value());
  EXPECT_EQ(*ntp, 0xAABBCCDD11223344ULL);
}

TEST(RtpHeaderExtensionParserTest, ParseMultipleElements)
{
  // ID=1, L=1 (2 bytes) + ID=3, L=0 (1 byte)
  std::vector<uint8_t> ext_data = {
    0x11,
    0xAA,
    0xBB, // ID=1, L=1 → 2 bytes
    0x30,
    0xCC, // ID=3, L=0 → 1 byte
    0x00,
    0x00,
    0x00 // 패딩
  };

  auto elements = nx::rtp::parse_one_byte_extension(ext_data);
  ASSERT_EQ(elements.size(), 2u);
  EXPECT_EQ(elements[0].id, 1);
  EXPECT_EQ(elements[0].data.size(), 2u);
  EXPECT_EQ(elements[1].id, 3);
  EXPECT_EQ(elements[1].data.size(), 1u);
}

TEST(RtpHeaderExtensionParserTest, ExtractNtp64_WrongId_ReturnsNullopt)
{
  std::vector<nx::rtp::RtpHeaderExtElement> elements;
  nx::rtp::RtpHeaderExtElement elem;
  elem.id = 2;
  elem.data.resize(8, 0xFF);
  elements.push_back(std::move(elem));

  auto result = nx::rtp::extract_ntp64(elements, 1);
  EXPECT_FALSE(result.has_value());
}

TEST(RtpHeaderExtensionParserTest, ExtractNtp64_WrongSize_ReturnsNullopt)
{
  std::vector<nx::rtp::RtpHeaderExtElement> elements;
  nx::rtp::RtpHeaderExtElement elem;
  elem.id = 1;
  elem.data.resize(4, 0xFF); // 4 bytes (NTP-64는 8 bytes 필요)
  elements.push_back(std::move(elem));

  auto result = nx::rtp::extract_ntp64(elements, 1);
  EXPECT_FALSE(result.has_value());
}

// ============================================================================
// Packetizer + Extension 통합 테스트
// ============================================================================

TEST(RtpPacketizerExtensionTest, H264_SingleNal_WithExtension)
{
  nx::rtp::RtpH264Packetizer packetizer;
  packetizer.set_ssrc(42);
  packetizer.set_payload_type(96);

  // 작은 NAL (Single NAL 모드): Annex B start code + NAL
  std::vector<uint8_t> frame = {0x00, 0x00, 0x00, 0x01, 0x65, 0x01, 0x02, 0x03};

  uint64_t ntp_value = 0xDEADBEEF12345678ULL;
  nx::rtp::RtpHeaderExtensionBuilder ext;
  ext.add_ntp64(1, ntp_value);

  nx::rtp::RtpFrameBuffer fb;
  packetizer.packetize(frame, 90000, true, ext, fb);
  std::vector<std::vector<uint8_t>> packets;
  for (size_t i = 0; i < fb.packet_count(); ++i) {
    auto pkt = fb.packet(i);
    packets.emplace_back(pkt.begin(), pkt.end());
  }

  ASSERT_EQ(packets.size(), 1u);
  auto& pkt = packets[0];

  // RTP 파싱으로 검증
  auto result = nx::rtp::RtpPacket::parse(pkt);
  ASSERT_TRUE(result.has_value());

  const auto& header = result->header();
  EXPECT_TRUE(header.extension);
  EXPECT_EQ(header.ssrc, 42u);
  EXPECT_EQ(header.payload_type, 96);
  EXPECT_EQ(header.timestamp, 90000u);

  // 확장 헤더 검증
  ASSERT_TRUE(header.ext_header.has_value());
  EXPECT_EQ(header.ext_header->profile, 0xBEDE);

  // One-Byte 파싱으로 NTP-64 추출
  auto elements = nx::rtp::parse_one_byte_extension(header.ext_header->data);
  auto ntp = nx::rtp::extract_ntp64(elements, 1);
  ASSERT_TRUE(ntp.has_value());
  EXPECT_EQ(*ntp, ntp_value);

  // 페이로드: NAL 데이터 (start code 제거)
  EXPECT_EQ(result->payload().size(), 4u); // 0x65, 0x01, 0x02, 0x03
}

TEST(RtpPacketizerExtensionTest, H264_WithoutExtension_NoXBit)
{
  nx::rtp::RtpH264Packetizer packetizer;
  packetizer.set_ssrc(1);

  std::vector<uint8_t> frame = {0x00, 0x00, 0x00, 0x01, 0x65, 0xAA};

  nx::rtp::RtpFrameBuffer fb;
  packetizer.packetize(frame, 1000, false, fb);
  std::vector<std::vector<uint8_t>> packets;
  for (size_t i = 0; i < fb.packet_count(); ++i) {
    auto pkt = fb.packet(i);
    packets.emplace_back(pkt.begin(), pkt.end());
  }

  ASSERT_EQ(packets.size(), 1u);

  auto result = nx::rtp::RtpPacket::parse(packets[0]);
  ASSERT_TRUE(result.has_value());
  EXPECT_FALSE(result->header().extension);
  EXPECT_FALSE(result->header().ext_header.has_value());
}

TEST(RtpPacketizerExtensionTest, G711_WithExtension)
{
  nx::rtp::RtpG711Packetizer packetizer;
  packetizer.set_ssrc(7);

  // 160 bytes (20ms @ 8kHz)
  std::vector<uint8_t> audio_data(160, 0x55);

  uint64_t ntp_value = 0x1122334455667788ULL;
  nx::rtp::RtpHeaderExtensionBuilder ext;
  ext.add_ntp64(1, ntp_value);

  nx::rtp::RtpFrameBuffer fb;
  packetizer.packetize(audio_data, 8000, false, ext, fb);
  std::vector<std::vector<uint8_t>> packets;
  for (size_t i = 0; i < fb.packet_count(); ++i) {
    auto pkt = fb.packet(i);
    packets.emplace_back(pkt.begin(), pkt.end());
  }

  ASSERT_GE(packets.size(), 1u);

  // 모든 패킷에 확장 헤더가 있어야 한다
  for (const auto& pkt : packets) {
    auto result = nx::rtp::RtpPacket::parse(pkt);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->header().extension);
    ASSERT_TRUE(result->header().ext_header.has_value());
    EXPECT_EQ(result->header().ext_header->profile, 0xBEDE);

    auto elements
      = nx::rtp::parse_one_byte_extension(result->header().ext_header->data);
    auto ntp = nx::rtp::extract_ntp64(elements, 1);
    ASSERT_TRUE(ntp.has_value());
    EXPECT_EQ(*ntp, ntp_value);
  }
}

TEST(RtpPacketizerExtensionTest, H264_FuA_WithExtension)
{
  nx::rtp::RtpH264Packetizer packetizer(200); // 작은 MTU로 FU-A 강제
  packetizer.set_ssrc(10);

  // Annex B start code + 큰 NAL (FU-A 강제)
  std::vector<uint8_t> frame;
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x00);
  frame.push_back(0x01);
  frame.push_back(0x65); // NAL type 5 (IDR)
  frame.resize(500, 0xAB);

  uint64_t ntp_value = 0xAAAABBBBCCCCDDDDULL;
  nx::rtp::RtpHeaderExtensionBuilder ext;
  ext.add_ntp64(1, ntp_value);

  nx::rtp::RtpFrameBuffer fb;
  packetizer.packetize(frame, 45000, true, ext, fb);
  std::vector<std::vector<uint8_t>> packets;
  for (size_t i = 0; i < fb.packet_count(); ++i) {
    auto pkt = fb.packet(i);
    packets.emplace_back(pkt.begin(), pkt.end());
  }

  // FU-A로 분할되므로 여러 패킷
  ASSERT_GT(packets.size(), 1u);

  // 모든 FU-A 패킷에 확장 헤더 존재 확인
  for (const auto& pkt : packets) {
    auto result = nx::rtp::RtpPacket::parse(pkt);
    ASSERT_TRUE(result.has_value());
    EXPECT_TRUE(result->header().extension);

    auto elements
      = nx::rtp::parse_one_byte_extension(result->header().ext_header->data);
    auto ntp = nx::rtp::extract_ntp64(elements, 1);
    ASSERT_TRUE(ntp.has_value());
    EXPECT_EQ(*ntp, ntp_value);
  }
}

// ============================================================================
// 빌더 → 직렬화 → 파싱 왕복 테스트
// ============================================================================

TEST(RtpHeaderExtensionTest, RoundTrip)
{
  nx::rtp::RtpHeaderExtensionBuilder builder;
  uint64_t original_ntp = 0xFEDCBA9876543210ULL;
  builder.add_ntp64(1, original_ntp);

  std::vector<uint8_t> serialized;
  builder.serialize(serialized);

  // 프로필 + length 건너뛰기 (4 bytes)
  std::span<const uint8_t> content(serialized.data() + 4, serialized.size() - 4);
  auto elements = nx::rtp::parse_one_byte_extension(content);
  auto ntp = nx::rtp::extract_ntp64(elements, 1);

  ASSERT_TRUE(ntp.has_value());
  EXPECT_EQ(*ntp, original_ntp);
}
