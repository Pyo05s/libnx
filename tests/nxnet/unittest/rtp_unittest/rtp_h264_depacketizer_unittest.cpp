// 파일: rtp_h264_depacketizer_unittest.cpp
// 생성일: 2026-02-23
// 설명: H.264 RTP 디패킷타이저 단위 테스트

#include "nxnet/rtp/rtp_h264_depacketizer.h"
#include "nxnet/rtp/rtp_h264_packetizer.h"
#include <gtest/gtest.h>


class H264DepacketizerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_depacketizer = std::make_unique<nx::rtp::RtpH264Depacketizer>();
  }

  nx::rtp::RtpHeaderView
  make_header(uint16_t seq, uint32_t timestamp, bool marker = false)
  {
    nx::rtp::RtpHeaderView header;
    header.version = 2;
    header.marker = marker;
    header.payload_type = 96;
    header.sequence_number = seq;
    header.timestamp = timestamp;
    header.ssrc = 1;
    return header;
  }

  std::unique_ptr<nx::rtp::RtpH264Depacketizer> m_depacketizer;
};

// ============================================================================
// Single NAL Unit 테스트
// ============================================================================

TEST_F(H264DepacketizerTest, SingleNalIdrSlice)
{
  // IDR NAL (type=5): 키프레임
  std::vector<uint8_t> payload = {0x65, 0x01, 0x02, 0x03};

  auto header = make_header(1000, 12345, true);

  std::vector<uint8_t> frame;
  bool keyframe = false;

  bool complete = m_depacketizer->process_packet(header, payload, frame, keyframe);

  EXPECT_TRUE(complete);
  EXPECT_TRUE(keyframe);
  // Annex B: start code (4) + NAL data (4) = 8
  ASSERT_EQ(frame.size(), 8);
  // 4-byte Annex B start code
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x00);
  EXPECT_EQ(frame[3], 0x01);
  EXPECT_EQ(frame[4], 0x65);
}

TEST_F(H264DepacketizerTest, SingleNalNonIdr)
{
  // Non-IDR NAL (type=1)
  std::vector<uint8_t> payload = {0x41, 0x01, 0x02};

  auto header = make_header(1000, 12345, true);

  std::vector<uint8_t> frame;
  bool keyframe = false;

  bool complete = m_depacketizer->process_packet(header, payload, frame, keyframe);

  EXPECT_TRUE(complete);
  EXPECT_FALSE(keyframe);
}

// ============================================================================
// FU-A (Fragmentation Unit) 테스트
// ============================================================================

TEST_F(H264DepacketizerTest, FuAFragmentation)
{
  // FU-A 첫 번째 조각 (Start bit 설정, IDR)
  // FU indicator: F=0, NRI=3, Type=28 (FU-A) -> 0x7C
  // FU header: S=1, E=0, R=0, Type=5 (IDR) -> 0x85
  std::vector<uint8_t> payload1 = {0x7C, 0x85, 0x01, 0x02, 0x03};
  auto header1 = make_header(1000, 12345);

  std::vector<uint8_t> frame;
  bool keyframe = false;
  bool complete = m_depacketizer->process_packet(header1, payload1, frame, keyframe);
  EXPECT_FALSE(complete);

  // FU-A 중간 조각
  // FU header: S=0, E=0, R=0, Type=5 -> 0x05
  std::vector<uint8_t> payload2 = {0x7C, 0x05, 0x04, 0x05, 0x06};
  auto header2 = make_header(1001, 12345);

  complete = m_depacketizer->process_packet(header2, payload2, frame, keyframe);
  EXPECT_FALSE(complete);

  // FU-A 마지막 조각 (End bit 설정, marker 설정)
  // FU header: S=0, E=1, R=0, Type=5 -> 0x45
  std::vector<uint8_t> payload3 = {0x7C, 0x45, 0x07, 0x08, 0x09};
  auto header3 = make_header(1002, 12345, true);

  complete = m_depacketizer->process_packet(header3, payload3, frame, keyframe);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(keyframe);

  // Annex B: start code (4) + NAL 헤더 (1) + 페이로드 (3+3+3 = 9) = 14
  ASSERT_EQ(frame.size(), 14);
  // 4-byte Annex B start code
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x00);
  EXPECT_EQ(frame[3], 0x01);
  // NAL 헤더: (FU indicator & 0xE0) | (FU header & 0x1F) = 0x60 | 0x05 = 0x65
  EXPECT_EQ(frame[4], 0x65);
}

// ============================================================================
// STAP-A (Single-Time Aggregation Packet) 테스트
// ============================================================================

TEST_F(H264DepacketizerTest, StapAPacket)
{
  // STAP-A: SPS + PPS를 하나의 패킷에 포함
  // STAP-A header: type=24 -> 0x18
  std::vector<uint8_t> payload;
  payload.push_back(0x18); // STAP-A 헤더

  // SPS (type=7)
  std::vector<uint8_t> sps = {0x67, 0x42, 0x00, 0x1F};
  payload.push_back(static_cast<uint8_t>((sps.size() >> 8) & 0xFF));
  payload.push_back(static_cast<uint8_t>(sps.size() & 0xFF));
  payload.insert(payload.end(), sps.begin(), sps.end());

  // PPS (type=8)
  std::vector<uint8_t> pps = {0x68, 0xCE, 0x38, 0x80};
  payload.push_back(static_cast<uint8_t>((pps.size() >> 8) & 0xFF));
  payload.push_back(static_cast<uint8_t>(pps.size() & 0xFF));
  payload.insert(payload.end(), pps.begin(), pps.end());

  auto header = make_header(1000, 12345, true);

  std::vector<uint8_t> frame;
  bool keyframe = false;

  bool complete = m_depacketizer->process_packet(header, payload, frame, keyframe);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(keyframe); // IDR 슬라이스만 키프레임으로 취급

  // Annex B: (start code + SPS) + (start code + PPS)
  // (4 + 4) + (4 + 4) = 16
  EXPECT_EQ(frame.size(), 16);
}

// ============================================================================
// Reset 테스트
// ============================================================================

TEST_F(H264DepacketizerTest, ResetClearsState)
{
  // FU-A 시작 조각 전송
  std::vector<uint8_t> payload1 = {0x7C, 0x85, 0x01, 0x02};
  auto header1 = make_header(1000, 12345);

  std::vector<uint8_t> frame;
  bool keyframe = false;
  m_depacketizer->process_packet(header1, payload1, frame, keyframe);

  // 리셋
  m_depacketizer->reset();

  // 리셋 후 새 패킷 정상 처리
  std::vector<uint8_t> payload2 = {0x41, 0x01, 0x02};
  auto header2 = make_header(2000, 22222, true);

  bool complete = m_depacketizer->process_packet(header2, payload2, frame, keyframe);
  EXPECT_TRUE(complete);
}

// ============================================================================
// SEI NAL 보존 테스트 (Annex B 포맷)
// ============================================================================

TEST_F(H264DepacketizerTest, SeiNalPreservedInAnnexBOutput)
{
  // SEI NAL (type=6): user_data_unregistered (payloadType=5)
  // 내부 payload는 emulation prevention byte로 start code 오인식을 방지
  std::vector<uint8_t> sei_payload = {
    0x06, // NAL 헤더 (type=6, SEI)
    0x05, // payloadType = 5 (user_data_unregistered)
    0x20, // payloadSize = 32
    // UUID (16 bytes)
    0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
    0x0F, 0x10,
    // user_data (16 bytes) — 가짜 start code 패턴 포함
    0xAA, 0xBB, 0x00, 0x00, 0x03, 0x01, 0x65, 0xCC, 0x00, 0x00, 0x00, 0x03, 0x01, 0x67,
    0xEE, 0xFF, 0x42
  };

  auto header = make_header(1000, 12345, true);
  std::vector<uint8_t> frame;
  bool keyframe = false;

  bool complete = m_depacketizer->process_packet(header, sei_payload, frame, keyframe);
  EXPECT_TRUE(complete);

  // Annex B 출력: 4-byte start code + SEI 데이터 전체
  ASSERT_EQ(frame.size(), 4 + sei_payload.size());

  // start code 확인
  EXPECT_EQ(frame[0], 0x00);
  EXPECT_EQ(frame[1], 0x00);
  EXPECT_EQ(frame[2], 0x00);
  EXPECT_EQ(frame[3], 0x01);

  // SEI 데이터가 잘림 없이 보존되었는지 확인
  EXPECT_EQ(std::vector<uint8_t>(frame.begin() + 4, frame.end()), sei_payload);
}

TEST_F(H264DepacketizerTest, SeiWithEmulationPrevention_PacketizerRoundTrip)
{
  // SPS + SEI(emulation prevention 포함) + IDR을 순차 전송
  // 1) SPS (single NAL, same timestamp)
  std::vector<uint8_t> sps = {0x67, 0x42, 0x00, 0x1F};
  auto h1 = make_header(1, 90000);
  std::vector<uint8_t> frame;
  bool keyframe = false;
  m_depacketizer->process_packet(h1, sps, frame, keyframe);

  // 2) SEI — 내부 00 00 01 패턴은 00 00 03 01로 이스케이프된 상태
  std::vector<uint8_t> sei = {0x06, 0x05, 0x14, // NAL=SEI, payloadType=5, size=20
                              0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                              0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E,
                              0x0F, 0x10, 0x00, 0x00, 0x03, 0x01, 0x65};
  auto h2 = make_header(2, 90000);
  m_depacketizer->process_packet(h2, sei, frame, keyframe);

  // 3) IDR (marker 설정 → 프레임 완료)
  std::vector<uint8_t> idr = {0x65, 0x01, 0x02, 0x03};
  auto h3 = make_header(3, 90000, true);
  bool complete = m_depacketizer->process_packet(h3, idr, frame, keyframe);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(keyframe);

  // 패킷타이저로 NAL 추출 (Annex B)
  auto nals =
    nx::rtp::RtpH264Packetizer::extract_nal_units(std::span<const uint8_t>(frame));

  // 정확히 3개 NAL (SPS, SEI, IDR) — SEI가 잘리지 않아야 함
  ASSERT_EQ(nals.size(), 3u);

  // NAL 타입 확인
  EXPECT_EQ(nals[0][0] & 0x1F, 7u); // SPS
  EXPECT_EQ(nals[1][0] & 0x1F, 6u); // SEI
  EXPECT_EQ(nals[2][0] & 0x1F, 5u); // IDR

  // SEI가 원본과 동일한지 확인 (잘림 없음)
  EXPECT_EQ(nals[1].size(), sei.size());
  EXPECT_EQ(std::vector<uint8_t>(nals[1].begin(), nals[1].end()), sei);
}

// ============================================================================
// Annex B 포맷 파싱 테스트 (패킷타이저)
// ============================================================================

TEST_F(H264DepacketizerTest, PacketizerExtractNalUnitsAnnexB)
{
  // Annex B 포맷: [00 00 00 01] [NAL 데이터] 반복
  std::vector<uint8_t> annexb_frame = {
    0x00, 0x00, 0x00, 0x01, 0x67, 0x42, 0x00,
    0x1F, // SPS
    0x00, 0x00, 0x00, 0x01, 0x68, 0xCE, 0x38,
    0x80 // PPS
  };

  auto nals =
    nx::rtp::RtpH264Packetizer::extract_nal_units(std::span<const uint8_t>(annexb_frame));

  ASSERT_EQ(nals.size(), 2u);
  EXPECT_EQ(nals[0][0], 0x67); // SPS
  EXPECT_EQ(nals[1][0], 0x68); // PPS
}

// ============================================================================
// 타임스탬프 변경 + marker 비트 동시 발생 시 프레임 손실 방지 테스트
// ============================================================================

TEST_F(H264DepacketizerTest, TimestampChangeWithMarkerNoFrameLoss)
{
  // 프레임 1: 여러 패킷 (marker로 완료)
  std::vector<uint8_t> sps = {0x67, 0x42, 0x00, 0x1F};
  std::vector<uint8_t> idr = {0x65, 0x01, 0x02};

  auto h1 = make_header(1, 1000);
  auto h2 = make_header(2, 1000, true);

  std::vector<uint8_t> frame;
  bool keyframe = false;

  m_depacketizer->process_packet(h1, sps, frame, keyframe);
  bool complete = m_depacketizer->process_packet(h2, idr, frame, keyframe);
  EXPECT_TRUE(complete);
  EXPECT_TRUE(keyframe);

  size_t frame1_size = frame.size();
  EXPECT_GT(frame1_size, 0u);

  // 프레임 2: 단일 패킷 (새 타임스탬프 + marker)
  // 이전 구현에서는 프레임 1이 손실되고 프레임 2만 출력되는 버그가 있었음
  std::vector<uint8_t> p_frame = {0x41, 0x01, 0x02, 0x03, 0x04};
  auto h3 = make_header(3, 2000, true);

  // 프레임 1은 이미 marker로 완료 → 버퍼가 비어있으므로
  // 프레임 2가 즉시 완료
  complete = m_depacketizer->process_packet(h3, p_frame, frame, keyframe);
  EXPECT_TRUE(complete);
  EXPECT_FALSE(keyframe);
  EXPECT_EQ(frame.size(), 4 + p_frame.size()); // Annex B: start code + NAL
}
