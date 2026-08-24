// 파일: rtp_h264_packetizer.cpp
// 생성일: 2026-02-26
// 설명: H.264 RTP 패킷타이저 구현 (RFC 6184)

#include "rtp_h264_packetizer.h"

#include <algorithm>
#include <array>
#include <cstring>

namespace nx::rtp {

namespace {

// 확장 헤더를 포함한 실효 RTP 헤더 크기 계산
uint16_t
effective_header_size(const RtpHeaderExtensionBuilder* ext)
{
  constexpr uint16_t kBaseHeaderSize = 12;
  if (ext && !ext->empty()) {
    return kBaseHeaderSize + static_cast<uint16_t>(ext->serialized_size());
  }
  return kBaseHeaderSize;
}

} // anonymous namespace

// ============================================================================
// 생성자
// ============================================================================

RtpH264Packetizer::RtpH264Packetizer(uint16_t max_packet_size)
    : m_max_payload_size(max_packet_size - kRtpHeaderSize)
{
}

// ============================================================================
// 패킷화
// ============================================================================

void
RtpH264Packetizer::packetize(
  std::span<const uint8_t> frame_data,
  uint32_t timestamp,
  bool /*is_keyframe*/,
  RtpFrameBuffer& output)
{
  if (frame_data.empty()) {
    return;
  }

  // Annex B Start Code 기반으로 NAL 유닛 추출
  auto nals = extract_nal_units(frame_data);
  if (nals.empty()) {
    return;
  }

  // 출력 버퍼 용량 사전 확보 (프레임 데이터 + RTP 헤더 오버헤드)
  size_t estimated_packets = nals.size() + frame_data.size() / m_max_payload_size;
  auto header_overhead = effective_header_size(m_current_ext);
  output.reserve(
    frame_data.size() + estimated_packets * header_overhead, estimated_packets);

  // 작은 NAL 유닛들을 STAP-A로 집합 가능한지 확인
  // 큰 NAL은 개별 처리 (Single NAL 또는 FU-A)
  std::vector<std::span<const uint8_t>> stap_candidates;
  uint32_t stap_total_size = 0;

  for (size_t i = 0; i < nals.size(); ++i) {
    const auto& nal = nals[i];
    bool is_last_nal = (i == nals.size() - 1);

    // STAP-A 집합 시 각 NAL 앞에 2바이트 길이 필드 + 1바이트 STAP-A 헤더
    uint32_t nal_stap_size = static_cast<uint32_t>(nal.size()) + 2; // 길이 필드 + NAL

    if (nal.size() <= m_max_payload_size) {
      // 집합 가능한 크기
      uint32_t new_stap_size = stap_total_size + nal_stap_size;
      if (stap_total_size == 0) {
        new_stap_size += 1; // STAP-A 헤더 1바이트
      }

      if (new_stap_size <= m_max_payload_size && !stap_candidates.empty()) {
        // 집합에 추가
        stap_candidates.push_back(nal);
        stap_total_size = new_stap_size;
      }
      else if (!stap_candidates.empty()) {
        // 기존 집합 전송 후 새 집합 시작
        bool prev_marker = false;
        if (stap_candidates.size() > 1) {
          send_stap_a(stap_candidates, timestamp, prev_marker, output);
        }
        else {
          send_single_nal(stap_candidates[0], timestamp, prev_marker, output);
        }
        stap_candidates.clear();
        stap_candidates.push_back(nal);
        stap_total_size = nal_stap_size + 1;
      }
      else {
        // 첫 집합 시작
        stap_candidates.push_back(nal);
        stap_total_size = nal_stap_size + 1;
      }
    }
    else {
      // 큰 NAL: 기존 집합 먼저 전송
      if (!stap_candidates.empty()) {
        if (stap_candidates.size() > 1) {
          send_stap_a(stap_candidates, timestamp, false, output);
        }
        else {
          send_single_nal(stap_candidates[0], timestamp, false, output);
        }
        stap_candidates.clear();
        stap_total_size = 0;
      }

      // FU-A 분할
      send_fu_a(nal, timestamp, is_last_nal, output);
    }

    // 마지막 NAL이면 남은 집합 전송
    if (is_last_nal && !stap_candidates.empty()) {
      if (stap_candidates.size() > 1) {
        send_stap_a(stap_candidates, timestamp, true, output);
      }
      else {
        send_single_nal(stap_candidates[0], timestamp, true, output);
      }
    }
  }

  // 모든 패킷 기록 완료 — sentinel 오프셋 추가
  output.finalize();
}

// ============================================================================
// 패킷화 (RFC 5285 확장 헤더 포함)
// ============================================================================

void
RtpH264Packetizer::packetize(
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

// ============================================================================
// NAL 유닛 추출 (내부 유통 포맷: Annex B)
// ============================================================================

std::vector<std::span<const uint8_t>>
RtpH264Packetizer::extract_nal_units(std::span<const uint8_t> frame_data)
{
  return extract_nal_units_annex_b(frame_data);
}

// ============================================================================
// Annex B 스타일 NAL 추출 (Start Code 기반)
// ============================================================================

std::vector<std::span<const uint8_t>>
RtpH264Packetizer::extract_nal_units_annex_b(std::span<const uint8_t> frame_data)
{
  std::vector<std::span<const uint8_t>> nals;
  const auto* data = frame_data.data();
  const auto size = frame_data.size();

  // Start Code 탐색 함수 (0x000001 또는 0x00000001)
  auto find_start_code = [&](size_t start) -> std::pair<size_t, size_t>
  {
    for (size_t i = start; i + 2 < size; ++i) {
      if (data[i] == 0x00 && data[i + 1] == 0x00) {
        if (data[i + 2] == 0x01) {
          return {i, 3}; // 3-byte start code
        }
        if (i + 3 < size && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
          return {i, 4}; // 4-byte start code
        }
      }
    }
    return {size, 0}; // 미발견
  };

  // 첫 Start Code 찾기
  auto [first_pos, first_len] = find_start_code(0);
  if (first_len == 0) {
    // Start Code 없음 → 전체가 하나의 NAL
    if (!frame_data.empty()) {
      nals.push_back(frame_data);
    }
    return nals;
  }

  size_t pos = first_pos + first_len;

  while (pos < size) {
    auto [next_pos, next_len] = find_start_code(pos);

    // 현재 NAL: pos ~ next_pos
    if (next_pos > pos) {
      nals.push_back(frame_data.subspan(pos, next_pos - pos));
    }

    if (next_len == 0) {
      break;
    }
    pos = next_pos + next_len;
  }

  return nals;
}

// ============================================================================
// Single NAL Unit — RtpFrameBuffer에 직접 기록
// ============================================================================

void
RtpH264Packetizer::send_single_nal(
  std::span<const uint8_t> nal, uint32_t timestamp, bool marker, RtpFrameBuffer& output)
{
  output.begin_packet();
  write_rtp_header(output, marker, timestamp);
  output.append(nal);
}

// ============================================================================
// FU-A 분할 — RtpFrameBuffer에 직접 기록
// ============================================================================

void
RtpH264Packetizer::send_fu_a(
  std::span<const uint8_t> nal, uint32_t timestamp, bool marker, RtpFrameBuffer& output)
{
  if (nal.empty()) {
    return;
  }

  // NAL 헤더 (1바이트): forbidden_zero_bit(1) | nal_ref_idc(2) | nal_unit_type(5)
  uint8_t nal_header = nal[0];
  uint8_t nal_ref_idc = nal_header & 0x60; // NRI 필드
  uint8_t nal_type = nal_header & 0x1F;    // NAL 유닛 타입

  // NAL 헤더 제거 (페이로드만 분할)
  auto payload = nal.subspan(1);
  size_t offset = 0;

  // FU-A 헤더: FU indicator(1) + FU header(1) = 2바이트
  uint16_t max_fragment_size = m_max_payload_size - 2;
  // 확장 헤더 공간 차감
  if (m_current_ext && !m_current_ext->empty()) {
    auto ext_size = static_cast<uint16_t>(m_current_ext->serialized_size());
    max_fragment_size =
      (m_max_payload_size > ext_size + 2) ? m_max_payload_size - ext_size - 2 : 1;
  }

  bool is_first = true;
  while (offset < payload.size()) {
    size_t fragment_size =
      std::min(static_cast<size_t>(max_fragment_size), payload.size() - offset);
    bool is_last = (offset + fragment_size >= payload.size());

    // FU indicator: forbidden_zero_bit(0) | nal_ref_idc | type(28=FU-A)
    uint8_t fu_indicator = nal_ref_idc | kNalFuA;

    // FU header: S(1) | E(1) | R(1) | Type(5)
    uint8_t fu_header = nal_type;
    if (is_first) {
      fu_header |= 0x80; // Start bit
    }
    if (is_last) {
      fu_header |= 0x40; // End bit
    }

    output.begin_packet();

    // RTP 헤더 (marker는 마지막 FU-A + 프레임 마지막 NAL일 때만)
    write_rtp_header(output, is_last && marker, timestamp);

    // FU indicator + FU header
    output.append(fu_indicator);
    output.append(fu_header);

    // 프래그먼트 데이터
    output.append(payload.subspan(offset, fragment_size));

    offset += fragment_size;
    is_first = false;
  }
}

// ============================================================================
// STAP-A 집합 — RtpFrameBuffer에 직접 기록
// ============================================================================

void
RtpH264Packetizer::send_stap_a(
  const std::vector<std::span<const uint8_t>>& nals,
  uint32_t timestamp,
  bool marker,
  RtpFrameBuffer& output)
{
  if (nals.empty()) {
    return;
  }

  output.begin_packet();
  write_rtp_header(output, marker, timestamp);

  // STAP-A 헤더: forbidden_zero_bit(0) | nal_ref_idc(최대값) | type(24)
  uint8_t max_nri = 0;
  for (const auto& nal : nals) {
    if (!nal.empty()) {
      uint8_t nri = nal[0] & 0x60;
      if (nri > max_nri) {
        max_nri = nri;
      }
    }
  }
  output.append(static_cast<uint8_t>(max_nri | kNalStapA));

  // 각 NAL 유닛: 2바이트 길이(Big-Endian) + NAL 데이터
  for (const auto& nal : nals) {
    output.append_u16_be(static_cast<uint16_t>(nal.size()));
    output.append(nal);
  }
}

// ============================================================================
// RTP 헤더 — RtpFrameBuffer에 직접 기록
// ============================================================================

void
RtpH264Packetizer::write_rtp_header(
  RtpFrameBuffer& output, bool marker, uint32_t timestamp)
{
  // V=2, P=0, X=ext?1:0, CC=0
  uint8_t byte0 = 0x80;
  if (m_current_ext && !m_current_ext->empty()) {
    byte0 |= 0x10; // X 비트 설정
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
    static_cast<uint8_t>(m_ssrc & 0xFF)
  };
  output.append(std::span<const uint8_t>{hdr});
  ++m_sequence_number;

  // RFC 5285 확장 헤더 직렬화
  if (m_current_ext && !m_current_ext->empty()) {
    // serialize()는 vector<uint8_t>에 append하므로 내부 data 참조를 직접 사용
    m_current_ext->serialize(output.data);
  }
}

// ============================================================================
// 설정 메서드
// ============================================================================

void
RtpH264Packetizer::reset()
{
  m_sequence_number = 0;
}

void
RtpH264Packetizer::set_ssrc(uint32_t ssrc)
{
  m_ssrc = ssrc;
}

void
RtpH264Packetizer::set_payload_type(uint8_t pt)
{
  m_payload_type = pt;
}

void
RtpH264Packetizer::set_max_packet_size(uint16_t size)
{
  m_max_payload_size = (size > kRtpHeaderSize) ? (size - kRtpHeaderSize) : 1;
}

} // namespace nx::rtp
