// 파일: rtp_raw_packetizer.cpp
// 생성일: 2026-03-18
// 설명: Raw RTP 패킷타이저 구현 — 코덱 비의존 패스스루

#include "rtp_raw_packetizer.h"

#include <algorithm>
#include <array>

namespace nx::rtp {

RtpRawPacketizer::RtpRawPacketizer(uint16_t max_packet_size)
    : m_max_payload_size(max_packet_size - kRtpHeaderSize)
{}

void
RtpRawPacketizer::packetize(
  std::span<const uint8_t> frame_data,
  uint32_t timestamp,
  bool /*is_keyframe*/,
  RtpFrameBuffer& output)
{
  if (frame_data.empty()) {
    return;
  }

  size_t offset = 0;
  auto payload_limit = m_max_payload_size;
  if (m_current_ext && !m_current_ext->empty()) {
    auto ext_size = static_cast<uint16_t>(m_current_ext->serialized_size());
    payload_limit
      = (m_max_payload_size > ext_size) ? m_max_payload_size - ext_size : 1;
  }

  while (offset < frame_data.size()) {
    size_t chunk_size
      = std::min(static_cast<size_t>(payload_limit), frame_data.size() - offset);
    bool is_last = (offset + chunk_size >= frame_data.size());

    output.begin_packet();

    // RTP 헤더 (marker: 프레임 마지막 패킷)
    write_rtp_header(output, is_last, timestamp);

    // 원시 페이로드 그대로 삽입
    output.append(frame_data.subspan(offset, chunk_size));
    offset += chunk_size;
  }

  output.finalize();
}

void
RtpRawPacketizer::packetize(
  std::span<const uint8_t> frame_data,
  uint32_t timestamp,
  bool is_keyframe,
  const RtpHeaderExtensionBuilder& ext,
  RtpFrameBuffer& output)
{
  m_current_ext = ext.empty() ? nullptr : &ext;
  packetize(frame_data, timestamp, is_keyframe, output);
  m_current_ext = nullptr;
}

void
RtpRawPacketizer::write_rtp_header(
  RtpFrameBuffer& output, bool marker, uint32_t timestamp)
{
  uint8_t byte0 = 0x80;
  if (m_current_ext && !m_current_ext->empty()) {
    byte0 |= 0x10;
  }
  // RTP 고정 헤더 12바이트를 로컬 배열에 조립 후 벌크 추가
  uint8_t byte1 = m_payload_type & 0x7F;
  if (marker) {
    byte1 |= 0x80;
  }
  std::array<uint8_t, 12> hdr = {
    byte0,
    byte1,
    static_cast<uint8_t>(m_sequence_number >> 8),
    static_cast<uint8_t>(m_sequence_number & 0xFF),
    static_cast<uint8_t>(timestamp >> 24),
    static_cast<uint8_t>(timestamp >> 16),
    static_cast<uint8_t>(timestamp >> 8),
    static_cast<uint8_t>(timestamp & 0xFF),
    static_cast<uint8_t>(m_ssrc >> 24),
    static_cast<uint8_t>(m_ssrc >> 16),
    static_cast<uint8_t>(m_ssrc >> 8),
    static_cast<uint8_t>(m_ssrc & 0xFF)};
  output.append(std::span<const uint8_t>{hdr});
  ++m_sequence_number;

  if (m_current_ext && !m_current_ext->empty()) {
    m_current_ext->serialize(output.data);
  }
}

void
RtpRawPacketizer::reset()
{
  m_sequence_number = 0;
}

void
RtpRawPacketizer::set_ssrc(uint32_t ssrc)
{
  m_ssrc = ssrc;
}

void
RtpRawPacketizer::set_payload_type(uint8_t pt)
{
  m_payload_type = pt;
}

void
RtpRawPacketizer::set_max_packet_size(uint16_t size)
{
  m_max_payload_size = size - kRtpHeaderSize;
}

} // namespace nx::rtp
