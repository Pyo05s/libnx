// 파일: rtp_receiver_unittest.cpp
// 생성일: 2026-03-18
// 설명: RTP 수신기 단위 테스트 (디패킷타이저 미설정 시 패스스루 포함)

#include <gtest/gtest.h>
#include "nxnet/rtp/rtp_receiver.h"

#include <vector>
#include <cstdint>
#include <span>

namespace {

// 유효한 RTP 패킷 생성 헬퍼
// V=2, P=0, X=0, CC=0, M=marker, PT=pt, Seq=seq, TS=ts, SSRC=ssrc
std::vector<uint8_t>
make_rtp_packet(
  uint8_t pt,
  uint16_t seq,
  uint32_t ts,
  uint32_t ssrc,
  std::span<const uint8_t> payload,
  bool marker = false)
{
  std::vector<uint8_t> packet;
  packet.reserve(12 + payload.size());

  // RTP 헤더 (12 bytes)
  packet.push_back(0x80); // V=2, P=0, X=0, CC=0
  packet.push_back(static_cast<uint8_t>((marker ? 0x80 : 0x00) | (pt & 0x7F)));
  packet.push_back(static_cast<uint8_t>(seq >> 8));
  packet.push_back(static_cast<uint8_t>(seq & 0xFF));
  packet.push_back(static_cast<uint8_t>(ts >> 24));
  packet.push_back(static_cast<uint8_t>(ts >> 16));
  packet.push_back(static_cast<uint8_t>(ts >> 8));
  packet.push_back(static_cast<uint8_t>(ts & 0xFF));
  packet.push_back(static_cast<uint8_t>(ssrc >> 24));
  packet.push_back(static_cast<uint8_t>(ssrc >> 16));
  packet.push_back(static_cast<uint8_t>(ssrc >> 8));
  packet.push_back(static_cast<uint8_t>(ssrc & 0xFF));

  // 페이로드
  packet.insert(packet.end(), payload.begin(), payload.end());
  return packet;
}

} // namespace

// ============================================================================
// 디패킷타이저 미설정 시 패스스루 테스트 (G.711 오디오 등)
// ============================================================================

class RtpReceiverPassthroughTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_receiver = std::make_unique<nx::rtp::RtpReceiver>(
      m_ioc,
      nx::rtp::RtpReceiver::TransportMode::kTcpInterleaved);

    // 디패킷타이저 미설정 (오디오 패스스루 시나리오)
    m_receiver->set_frame_callback(
      [this](nx::media::SharedFrameData frame, uint64_t timestamp, bool keyframe) {
        m_received_frames.push_back(
          {std::vector<uint8_t>(frame->begin(), frame->end()), timestamp, keyframe});
      });
  }

  struct ReceivedFrame
  {
    std::vector<uint8_t> data;
    uint64_t timestamp;
    bool keyframe;
  };

  AsioContext m_ioc;
  std::unique_ptr<nx::rtp::RtpReceiver> m_receiver;
  std::vector<ReceivedFrame> m_received_frames;
};

TEST_F(RtpReceiverPassthroughTest, AudioPayloadDelivered)
{
  // G.711 PCMU 오디오 데이터
  std::vector<uint8_t> audio_payload = {0xFF, 0x7F, 0x80, 0x00, 0x55, 0xAA};
  auto packet = make_rtp_packet(0, 1, 160, 1, audio_payload);

  m_receiver->feed_data(0, packet);

  ASSERT_EQ(m_received_frames.size(), 1u);
  EXPECT_EQ(m_received_frames[0].data, audio_payload);
  EXPECT_EQ(m_received_frames[0].timestamp, 160u);
  EXPECT_FALSE(m_received_frames[0].keyframe);
}

TEST_F(RtpReceiverPassthroughTest, MultiplePacketsDelivered)
{
  std::vector<uint8_t> payload1 = {0x01, 0x02, 0x03};
  std::vector<uint8_t> payload2 = {0x04, 0x05, 0x06};

  m_receiver->feed_data(0, make_rtp_packet(0, 1, 160, 1, payload1));
  m_receiver->feed_data(0, make_rtp_packet(0, 2, 320, 1, payload2));

  ASSERT_EQ(m_received_frames.size(), 2u);
  EXPECT_EQ(m_received_frames[0].data, payload1);
  EXPECT_EQ(m_received_frames[0].timestamp, 160u);
  EXPECT_EQ(m_received_frames[1].data, payload2);
  EXPECT_EQ(m_received_frames[1].timestamp, 320u);
}

TEST_F(RtpReceiverPassthroughTest, TimestampPreserved)
{
  std::vector<uint8_t> payload = {0x10};
  uint32_t expected_ts = 80000; // 10초 @8kHz

  m_receiver->feed_data(0, make_rtp_packet(0, 100, expected_ts, 2, payload));

  ASSERT_EQ(m_received_frames.size(), 1u);
  EXPECT_EQ(m_received_frames[0].timestamp, expected_ts);
}

TEST_F(RtpReceiverPassthroughTest, PcmaPayloadType)
{
  // PCMA (PT=8) 패스스루 확인
  std::vector<uint8_t> payload = {0xD5, 0x55};
  m_receiver->feed_data(0, make_rtp_packet(8, 1, 160, 1, payload));

  ASSERT_EQ(m_received_frames.size(), 1u);
  EXPECT_EQ(m_received_frames[0].data, payload);
}

TEST_F(RtpReceiverPassthroughTest, NoCallbackNoDelivery)
{
  // 콜백 미설정 시 데이터 드롭 (크래시 없음)
  auto receiver = std::make_unique<nx::rtp::RtpReceiver>(
    m_ioc,
    nx::rtp::RtpReceiver::TransportMode::kTcpInterleaved);
  // 콜백/디패킷타이저 미설정

  std::vector<uint8_t> payload = {0x01};
  auto packet = make_rtp_packet(0, 1, 160, 1, payload);

  // 크래시 없이 정상 처리되어야 함
  EXPECT_NO_THROW(receiver->feed_data(0, packet));
}

TEST_F(RtpReceiverPassthroughTest, StatisticsUpdated)
{
  std::vector<uint8_t> payload = {0x01, 0x02, 0x03, 0x04};
  m_receiver->feed_data(0, make_rtp_packet(0, 1, 160, 1, payload));
  m_receiver->feed_data(0, make_rtp_packet(0, 2, 320, 1, payload));

  auto stats = m_receiver->get_statistics();
  EXPECT_EQ(stats.packets_received, 2u);
}
