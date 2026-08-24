// 파일: rtp_g711_packetizer_unittest.cpp
// 생성일: 2026-03-03
// 설명: G.711 RTP 패킷타이저 단위 테스트

#include <gtest/gtest.h>

#include <nxnet/rtp/rtp_g711_packetizer.h>
#include <nxnet/rtp/rtp_frame_buffer.h>
#include <nxnet/rtp/rtp_packet.h>

#include <cstdint>
#include <numeric>
#include <vector>

namespace {

// ============================================================================
// 테스트 헬퍼
// ============================================================================

/// 지정된 크기의 G.711 테스트 프레임 생성 (8kHz, 1ch, 1byte/sample)
/// @param sample_count 샘플 수 (바이트 수와 동일)
std::vector<uint8_t>
make_g711_frame(size_t sample_count)
{
  std::vector<uint8_t> data(sample_count);
  std::iota(data.begin(), data.end(), static_cast<uint8_t>(0));
  return data;
}

/// RTP 패킷에서 헤더를 파싱하여 검증에 사용
struct ParsedRtpHeader
{
  uint8_t version = 0;
  bool marker = false;
  uint8_t payload_type = 0;
  uint16_t sequence_number = 0;
  uint32_t timestamp = 0;
  uint32_t ssrc = 0;
};

ParsedRtpHeader
parse_rtp_header(const std::vector<uint8_t>& packet)
{
  ParsedRtpHeader h;
  if (packet.size() < 12) {
    return h;
  }
  h.version = (packet[0] >> 6) & 0x03;
  h.marker = (packet[1] & 0x80) != 0;
  h.payload_type = packet[1] & 0x7F;
  h.sequence_number = (static_cast<uint16_t>(packet[2]) << 8) | packet[3];
  h.timestamp = (static_cast<uint32_t>(packet[4]) << 24)
                | (static_cast<uint32_t>(packet[5]) << 16)
                | (static_cast<uint32_t>(packet[6]) << 8) | packet[7];
  h.ssrc = (static_cast<uint32_t>(packet[8]) << 24)
           | (static_cast<uint32_t>(packet[9]) << 16)
           | (static_cast<uint32_t>(packet[10]) << 8) | packet[11];
  return h;
}

// ============================================================================
// G711 패킷타이저 테스트
// ============================================================================

class RtpG711PacketizerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_packetizer = std::make_unique<nx::rtp::RtpG711Packetizer>();
    m_packetizer->set_ssrc(100);
    m_packetizer->set_payload_type(8); // PCMA
  }

  std::unique_ptr<nx::rtp::RtpG711Packetizer> m_packetizer;
  std::vector<std::vector<uint8_t>> m_packets;

  void collect_packets(
    std::span<const uint8_t> data, uint32_t timestamp, bool is_keyframe = false)
  {
    m_packets.clear();
    nx::rtp::RtpFrameBuffer fb;
    m_packetizer->packetize(data, timestamp, is_keyframe, fb);
    for (size_t i = 0; i < fb.packet_count(); ++i) {
      auto pkt = fb.packet(i);
      m_packets.emplace_back(pkt.begin(), pkt.end());
    }
  }
};

TEST_F(RtpG711PacketizerTest, EmptyDataProducesNoPackets)
{
  std::vector<uint8_t> empty;
  collect_packets(empty, 0);
  EXPECT_TRUE(m_packets.empty());
}

TEST_F(RtpG711PacketizerTest, NullCallbackDoesNotCrash)
{
  auto data = make_g711_frame(160);
  nx::rtp::RtpFrameBuffer fb;
  EXPECT_NO_THROW(m_packetizer->packetize(data, 0, false, fb));
}

TEST_F(RtpG711PacketizerTest, SmallFrameSinglePacket)
{
  // 20ms @ 8kHz = 160 샘플 = 160 bytes (MTU 이내)
  auto data = make_g711_frame(160);
  collect_packets(data, 1000);

  ASSERT_EQ(m_packets.size(), 1u);

  // 패킷 크기 검증: RTP 헤더(12) + 페이로드(160) = 172
  EXPECT_EQ(m_packets[0].size(), 12u + 160u);

  auto hdr = parse_rtp_header(m_packets[0]);
  EXPECT_EQ(hdr.version, 2u);
  EXPECT_TRUE(hdr.marker);         // 단일 패킷이므로 마지막 = marker
  EXPECT_EQ(hdr.payload_type, 8u); // PCMA
  EXPECT_EQ(hdr.timestamp, 1000u);
  EXPECT_EQ(hdr.ssrc, 100u);
  EXPECT_EQ(hdr.sequence_number, 0u);
}

TEST_F(RtpG711PacketizerTest, PayloadMatchesInput)
{
  auto data = make_g711_frame(160);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 1u);

  // 페이로드 부분이 입력 데이터와 동일한지 검증
  auto payload
    = std::span<const uint8_t>(m_packets[0].data() + 12, m_packets[0].size() - 12);
  EXPECT_EQ(payload.size(), data.size());
  EXPECT_TRUE(std::equal(payload.begin(), payload.end(), data.begin()));
}

TEST_F(RtpG711PacketizerTest, LargeFrameMultiplePackets)
{
  // MTU 초과 데이터 (기본 max_payload = 1400 - 12 = 1388)
  // 3000 bytes → 3개 패킷 (1388 + 1388 + 224)
  auto data = make_g711_frame(3000);
  collect_packets(data, 5000);

  ASSERT_EQ(m_packets.size(), 3u);

  // 첫 두 패킷: marker=false
  EXPECT_FALSE(parse_rtp_header(m_packets[0]).marker);
  EXPECT_FALSE(parse_rtp_header(m_packets[1]).marker);
  // 마지막 패킷: marker=true
  EXPECT_TRUE(parse_rtp_header(m_packets[2]).marker);

  // 페이로드 크기 검증
  EXPECT_EQ(m_packets[0].size() - 12, 1388u);
  EXPECT_EQ(m_packets[1].size() - 12, 1388u);
  EXPECT_EQ(m_packets[2].size() - 12, 224u);
}

TEST_F(RtpG711PacketizerTest, TimestampIncrementPerChunk)
{
  // G.711은 1 sample = 1 byte, 클럭 8kHz
  // 분할 시 각 청크의 타임스탬프는 바이트 수만큼 증가
  auto data = make_g711_frame(3000);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 3u);

  auto ts0 = parse_rtp_header(m_packets[0]).timestamp;
  auto ts1 = parse_rtp_header(m_packets[1]).timestamp;
  auto ts2 = parse_rtp_header(m_packets[2]).timestamp;

  EXPECT_EQ(ts0, 0u);
  EXPECT_EQ(ts1, 1388u);         // 첫 청크 크기만큼 증가
  EXPECT_EQ(ts2, 1388u + 1388u); // 두 번째 청크 크기만큼 추가 증가
}

TEST_F(RtpG711PacketizerTest, SequenceNumberIncrement)
{
  auto data = make_g711_frame(160);

  // 첫 프레임
  collect_packets(data, 0);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 0u);

  // 두 번째 프레임 (시퀀스 번호가 1 증가)
  collect_packets(data, 160);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 1u);

  // 세 번째 프레임
  collect_packets(data, 320);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 2u);
}

TEST_F(RtpG711PacketizerTest, ResetClearsSequenceNumber)
{
  auto data = make_g711_frame(160);
  collect_packets(data, 0);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 0u);

  collect_packets(data, 160);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 1u);

  m_packetizer->reset();

  collect_packets(data, 320);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).sequence_number, 0u);
}

TEST_F(RtpG711PacketizerTest, SetPcmuPayloadType)
{
  m_packetizer->set_payload_type(0); // PCMU

  auto data = make_g711_frame(160);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 1u);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).payload_type, 0u);
}

TEST_F(RtpG711PacketizerTest, SetSsrc)
{
  m_packetizer->set_ssrc(12345);

  auto data = make_g711_frame(160);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 1u);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).ssrc, 12345u);
}

TEST_F(RtpG711PacketizerTest, CustomMaxPacketSize)
{
  // max_packet_size = 100 → max_payload = 88
  m_packetizer->set_max_packet_size(100);

  auto data = make_g711_frame(300);
  collect_packets(data, 0);

  // 300 / 88 = 3.4 → 4 패킷
  ASSERT_EQ(m_packets.size(), 4u);

  // 처음 3 패킷: 88 bytes 페이로드
  for (size_t i = 0; i < 3; ++i) {
    EXPECT_EQ(m_packets[i].size() - 12, 88u);
    EXPECT_FALSE(parse_rtp_header(m_packets[i]).marker);
  }
  // 마지막: 300 - 88*3 = 36 bytes
  EXPECT_EQ(m_packets[3].size() - 12, 36u);
  EXPECT_TRUE(parse_rtp_header(m_packets[3]).marker);
}

TEST_F(RtpG711PacketizerTest, ExactMtuBoundary)
{
  // max_payload = 1388 → 정확히 1388 바이트 프레임은 단일 패킷
  auto data = make_g711_frame(1388);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 1u);
  EXPECT_TRUE(parse_rtp_header(m_packets[0]).marker);
}

TEST_F(RtpG711PacketizerTest, OneByteBeyondMtu)
{
  // max_payload = 1388 → 1389 바이트는 2개 패킷
  auto data = make_g711_frame(1389);
  collect_packets(data, 0);

  ASSERT_EQ(m_packets.size(), 2u);
  EXPECT_EQ(m_packets[0].size() - 12, 1388u);
  EXPECT_EQ(m_packets[1].size() - 12, 1u);
}

TEST_F(RtpG711PacketizerTest, SingleByteFrame)
{
  auto data = make_g711_frame(1);
  collect_packets(data, 42);

  ASSERT_EQ(m_packets.size(), 1u);
  EXPECT_EQ(m_packets[0].size(), 13u); // 12 header + 1 payload
  EXPECT_TRUE(parse_rtp_header(m_packets[0]).marker);
  EXPECT_EQ(parse_rtp_header(m_packets[0]).timestamp, 42u);
}

TEST_F(RtpG711PacketizerTest, TotalPayloadMatchesOriginal)
{
  // 분할된 모든 페이로드를 합치면 원본과 동일해야 함
  auto data = make_g711_frame(5000);
  collect_packets(data, 0);

  std::vector<uint8_t> reassembled;
  for (const auto& pkt : m_packets) {
    reassembled.insert(reassembled.end(), pkt.begin() + 12, pkt.end());
  }

  EXPECT_EQ(reassembled.size(), data.size());
  EXPECT_TRUE(std::equal(reassembled.begin(), reassembled.end(), data.begin()));
}

} // anonymous namespace
