// 파일: rtp_header_extension.cpp
// 생성일: 2026-03-30
// 설명: RFC 5285 One-Byte RTP Header Extension 빌더 및 파서 구현

#include "rtp_header_extension.h"

namespace nx::rtp {

// ============================================================================
// RtpHeaderExtensionBuilder
// ============================================================================

void
RtpHeaderExtensionBuilder::add(uint8_t id, std::span<const uint8_t> data)
{
  // RFC 5285: ID 1~14, 데이터 1~16 bytes
  if (id < 1 || id > 14 || data.empty() || data.size() > 16) {
    return;
  }

  RtpHeaderExtElement element;
  element.id = id;
  element.data.assign(data.begin(), data.end());
  m_elements.push_back(std::move(element));
}

void
RtpHeaderExtensionBuilder::add_ntp64(uint8_t id, uint64_t ntp64)
{
  uint8_t buf[8];
  buf[0] = static_cast<uint8_t>(ntp64 >> 56);
  buf[1] = static_cast<uint8_t>(ntp64 >> 48);
  buf[2] = static_cast<uint8_t>(ntp64 >> 40);
  buf[3] = static_cast<uint8_t>(ntp64 >> 32);
  buf[4] = static_cast<uint8_t>(ntp64 >> 24);
  buf[5] = static_cast<uint8_t>(ntp64 >> 16);
  buf[6] = static_cast<uint8_t>(ntp64 >> 8);
  buf[7] = static_cast<uint8_t>(ntp64 & 0xFF);
  add(id, std::span<const uint8_t>(buf, 8));
}

size_t
RtpHeaderExtensionBuilder::serialized_size() const
{
  if (m_elements.empty()) {
    return 0;
  }

  // 4바이트 확장 블록 헤더 (profile + length)
  size_t content_bytes = 0;
  for (const auto& elem : m_elements) {
    // 1바이트 요소 헤더 (ID:4 | L:4) + 데이터
    content_bytes += 1 + elem.data.size();
  }

  // 32-bit 정렬 패딩
  size_t padded = (content_bytes + 3) & ~static_cast<size_t>(3);
  return 4 + padded;
}

void
RtpHeaderExtensionBuilder::serialize(std::vector<uint8_t>& packet) const
{
  if (m_elements.empty()) {
    return;
  }

  // 요소 콘텐츠 크기 계산
  size_t content_bytes = 0;
  for (const auto& elem : m_elements) {
    content_bytes += 1 + elem.data.size();
  }
  size_t padded = (content_bytes + 3) & ~static_cast<size_t>(3);
  uint16_t length_words = static_cast<uint16_t>(padded / 4);

  // 프로필 0xBEDE (2 bytes, Big-Endian)
  packet.push_back(static_cast<uint8_t>(kOneByteExtensionProfile >> 8));
  packet.push_back(static_cast<uint8_t>(kOneByteExtensionProfile & 0xFF));

  // Length (32-bit 워드 수, Big-Endian)
  packet.push_back(static_cast<uint8_t>(length_words >> 8));
  packet.push_back(static_cast<uint8_t>(length_words & 0xFF));

  // 각 요소: [ID(4) | L(4)] + data
  for (const auto& elem : m_elements) {
    uint8_t header_byte
      = static_cast<uint8_t>((elem.id << 4) | ((elem.data.size() - 1) & 0x0F));
    packet.push_back(header_byte);
    packet.insert(packet.end(), elem.data.begin(), elem.data.end());
  }

  // 32-bit 정렬 패딩 (ID=0 패딩 바이트)
  size_t padding = padded - content_bytes;
  for (size_t i = 0; i < padding; ++i) {
    packet.push_back(0x00);
  }
}

// ============================================================================
// 파싱
// ============================================================================

std::vector<RtpHeaderExtElement>
parse_one_byte_extension(std::span<const uint8_t> ext_data)
{
  std::vector<RtpHeaderExtElement> elements;
  size_t pos = 0;

  while (pos < ext_data.size()) {
    uint8_t byte = ext_data[pos];

    // ID=0: 패딩 바이트, 건너뜀
    if (byte == 0) {
      ++pos;
      continue;
    }

    // ID=15: 종료 마커
    uint8_t id = (byte >> 4) & 0x0F;
    if (id == 15) {
      break;
    }

    uint8_t len = (byte & 0x0F) + 1; // L 필드는 길이-1
    ++pos;

    if (pos + len > ext_data.size()) {
      break;
    }

    RtpHeaderExtElement elem;
    elem.id = id;
    elem.data.assign(ext_data.begin() + pos, ext_data.begin() + pos + len);
    elements.push_back(std::move(elem));
    pos += len;
  }

  return elements;
}

std::optional<uint64_t>
extract_ntp64(const std::vector<RtpHeaderExtElement>& elements, uint8_t id)
{
  for (const auto& elem : elements) {
    if (elem.id == id && elem.data.size() == 8) {
      uint64_t ntp = 0;
      for (int i = 0; i < 8; ++i) {
        ntp = (ntp << 8) | elem.data[i];
      }
      return ntp;
    }
  }
  return std::nullopt;
}

} // namespace nx::rtp
