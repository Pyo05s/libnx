// 파일: rtp_packet.cpp
// 생성일: 2026-02-23
// 설명: RTP 패킷 파싱 구현

#include "rtp_packet.h"

#include <cstring>

namespace nx::rtp {

namespace {

// 네트워크 바이트 순서(Big Endian) -> 호스트 바이트 순서 변환
inline uint16_t
read_u16_be(const uint8_t* data)
{
  return static_cast<uint16_t>((data[0] << 8) | data[1]);
}

inline uint32_t
read_u32_be(const uint8_t* data)
{
  return (static_cast<uint32_t>(data[0]) << 24) | (static_cast<uint32_t>(data[1]) << 16)
         | (static_cast<uint32_t>(data[2]) << 8) | static_cast<uint32_t>(data[3]);
}

} // anonymous namespace

nx::expected<RtpPacket>
RtpPacket::parse(std::span<const uint8_t> data)
{
  // RTP 최소 헤더 크기: 12바이트
  constexpr size_t kMinHeaderSize = 12;

  if (data.size() < kMinHeaderSize) {
    return std::unexpected(make_error_code(RtpErrc::packet_too_short));
  }

  RtpPacket packet;
  auto& header = packet.m_header;
  packet.m_total_size = data.size();

  // 첫 번째 바이트: V(2) P(1) X(1) CC(4)
  uint8_t byte0 = data[0];
  header.version = (byte0 >> 6) & 0x03;
  header.padding = ((byte0 >> 5) & 0x01) != 0;
  header.extension = ((byte0 >> 4) & 0x01) != 0;
  header.csrc_count = byte0 & 0x0F;

  if (header.version != 2) {
    return std::unexpected(make_error_code(RtpErrc::invalid_version));
  }

  // 두 번째 바이트: M(1) PT(7)
  uint8_t byte1 = data[1];
  header.marker = ((byte1 >> 7) & 0x01) != 0;
  header.payload_type = byte1 & 0x7F;

  // 시퀀스 번호 (2바이트)
  header.sequence_number = read_u16_be(&data[2]);

  // 타임스탬프 (4바이트)
  header.timestamp = read_u32_be(&data[4]);

  // SSRC (4바이트)
  header.ssrc = read_u32_be(&data[8]);

  size_t offset = kMinHeaderSize;

  // CSRC 목록
  size_t csrc_size = header.csrc_count * 4;
  if (data.size() < offset + csrc_size) {
    return std::unexpected(make_error_code(RtpErrc::packet_too_short));
  }

  header.csrc_list.reserve(header.csrc_count);
  for (uint8_t i = 0; i < header.csrc_count; ++i) {
    header.csrc_list.push_back(read_u32_be(&data[offset]));
    offset += 4;
  }

  // 확장 헤더
  if (header.extension) {
    if (data.size() < offset + 4) {
      return std::unexpected(make_error_code(RtpErrc::packet_too_short));
    }

    RtpExtension ext;
    ext.profile = read_u16_be(&data[offset]);
    uint16_t ext_length = read_u16_be(&data[offset + 2]); // 32비트 워드 단위
    offset += 4;

    size_t ext_bytes = ext_length * 4;
    if (data.size() < offset + ext_bytes) {
      return std::unexpected(make_error_code(RtpErrc::packet_too_short));
    }

    ext.data.assign(data.begin() + offset, data.begin() + offset + ext_bytes);
    header.ext_header = std::move(ext);
    offset += ext_bytes;
  }

  // 패딩 처리
  size_t payload_end = data.size();
  if (header.padding) {
    if (data.empty()) {
      return std::unexpected(make_error_code(RtpErrc::invalid_header));
    }
    uint8_t padding_length = data.back();
    if (padding_length == 0 || padding_length > payload_end - offset) {
      return std::unexpected(make_error_code(RtpErrc::invalid_header));
    }
    payload_end -= padding_length;
  }

  // 페이로드 추출
  if (offset <= payload_end) {
    packet.m_payload.assign(data.begin() + offset, data.begin() + payload_end);
  }

  return packet;
}

std::error_code
RtpPacket::parse_header_and_payload(
  std::span<const uint8_t> data,
  RtpHeaderView& out_header,
  std::span<const uint8_t>& out_payload) noexcept
{
  constexpr size_t kMinHeaderSize = 12;

  if (data.size() < kMinHeaderSize) {
    return make_error_code(RtpErrc::packet_too_short);
  }

  uint8_t byte0 = data[0];
  out_header.version = (byte0 >> 6) & 0x03;
  out_header.padding = ((byte0 >> 5) & 0x01) != 0;
  out_header.extension = ((byte0 >> 4) & 0x01) != 0;
  uint8_t csrc_count = byte0 & 0x0F;

  if (out_header.version != 2) {
    return make_error_code(RtpErrc::invalid_version);
  }

  uint8_t byte1 = data[1];
  out_header.marker = ((byte1 >> 7) & 0x01) != 0;
  out_header.payload_type = byte1 & 0x7F;

  out_header.sequence_number = read_u16_be(&data[2]);
  out_header.timestamp = read_u32_be(&data[4]);
  out_header.ssrc = read_u32_be(&data[8]);

  size_t offset = kMinHeaderSize;

  // CSRC 스킵 (파싱하지 않음)
  size_t csrc_size = static_cast<size_t>(csrc_count) * 4;
  if (data.size() < offset + csrc_size) {
    return make_error_code(RtpErrc::packet_too_short);
  }
  offset += csrc_size;

  // 확장 헤더 스킵 (파싱하지 않음)
  if (out_header.extension) {
    if (data.size() < offset + 4) {
      return make_error_code(RtpErrc::packet_too_short);
    }

    uint16_t ext_length = read_u16_be(&data[offset + 2]);
    offset += 4;

    size_t ext_bytes = static_cast<size_t>(ext_length) * 4;
    if (data.size() < offset + ext_bytes) {
      return make_error_code(RtpErrc::packet_too_short);
    }
    offset += ext_bytes;
  }

  size_t payload_end = data.size();
  if (out_header.padding) {
    if (data.empty()) {
      return make_error_code(RtpErrc::invalid_header);
    }
    uint8_t padding_length = data.back();
    if (padding_length == 0 || padding_length > payload_end - offset) {
      return make_error_code(RtpErrc::invalid_header);
    }
    payload_end -= padding_length;
  }

  if (offset > payload_end) {
    return make_error_code(RtpErrc::invalid_header);
  }

  out_payload = data.subspan(offset, payload_end - offset);
  return std::error_code{};
}

} // namespace nx::rtp
