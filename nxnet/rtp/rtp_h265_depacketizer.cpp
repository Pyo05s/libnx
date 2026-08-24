// 파일: rtp_h265_depacketizer.cpp
// 생성일: 2026-04-02
// 설명: H.265 RTP 디패킷타이저 구현 (RFC 7798)

#include "rtp_h265_depacketizer.h"

#include <spdlog/spdlog.h>

namespace nx::rtp {

RtpH265Depacketizer::RtpH265Depacketizer()
{
  m_accumulator.reserve(256 * 1024);  // 256KB 예약
  m_frame_buffer.reserve(512 * 1024); // 512KB 예약
}

bool
RtpH265Depacketizer::process_packet(
  const RtpHeaderView& header,
  std::span<const uint8_t> payload,
  std::vector<uint8_t>& out_frame,
  bool& out_keyframe)
{
  if (payload.size() < 2) {
    return false;
  }

  // 타임스탬프 변경 감지 -> 이전 프레임 출력
  if (
    m_last_timestamp != 0 && header.timestamp != m_last_timestamp
    && !m_frame_buffer.empty()) {
    out_frame.swap(m_frame_buffer);
    out_keyframe = m_frame_has_keyframe;
    m_frame_buffer.clear();
    m_frame_has_keyframe = false;

    // 현재 패킷을 새 프레임에 추가 (데이터 손실 방지)
    m_last_timestamp = header.timestamp;
    dispatch_nal(payload, header.timestamp);
    m_last_seq = header.sequence_number;

    // marker 비트 동시 설정 시 pending에 보관
    if (header.marker && !m_frame_buffer.empty()) {
      m_pending_frame.swap(m_frame_buffer);
      m_pending_keyframe = m_frame_has_keyframe;
      m_frame_buffer.clear();
      m_frame_has_keyframe = false;
      m_has_pending = true;
    }

    return true;
  }

  // pending 프레임 우선 처리
  if (m_has_pending) {
    out_frame.swap(m_pending_frame);
    out_keyframe = m_pending_keyframe;
    m_has_pending = false;

    m_last_timestamp = header.timestamp;
    dispatch_nal(payload, header.timestamp);
    m_last_seq = header.sequence_number;

    if (header.marker && !m_frame_buffer.empty()) {
      m_pending_frame.swap(m_frame_buffer);
      m_pending_keyframe = m_frame_has_keyframe;
      m_frame_buffer.clear();
      m_frame_has_keyframe = false;
      m_has_pending = true;
    }

    return true;
  }

  m_last_timestamp = header.timestamp;
  dispatch_nal(payload, header.timestamp);
  m_last_seq = header.sequence_number;

  // marker 비트 설정 시 프레임 완료 (swap: m_frame_buffer가 out_frame의 capacity를
  // 보존)
  if (header.marker && !m_frame_buffer.empty()) {
    out_frame.swap(m_frame_buffer);
    out_keyframe = m_frame_has_keyframe;
    m_frame_buffer.clear();
    m_frame_has_keyframe = false;
    return true;
  }

  return false;
}

void
RtpH265Depacketizer::reset()
{
  m_accumulator.clear();
  m_frame_buffer.clear();
  m_pending_frame.clear();
  m_last_seq = 0;
  m_last_timestamp = 0;
  m_in_fragmentation = false;
  m_frame_has_keyframe = false;
  m_has_pending = false;
  m_pending_keyframe = false;
  reset_error_count();
}

void
RtpH265Depacketizer::dispatch_nal(
  std::span<const uint8_t> payload, uint32_t rtp_timestamp)
{
  if (payload.size() < 2) {
    return;
  }

  uint8_t nal_type = extract_nal_type(payload[0]);

  if (nal_type == static_cast<uint8_t>(NalUnitType::kFu)) {
    handle_fu(payload);
  }
  else if (nal_type == static_cast<uint8_t>(NalUnitType::kAp)) {
    handle_ap(payload);
  }
  else if (nal_type <= 47) {
    // 유효한 H.265 NAL 타입 범위 (0~47)
    handle_single_nal(payload);
  }
  else {
    spdlog::warn("H.265 알 수 없는 NAL 타입: {}", nal_type);
    report_unknown_nal(nal_type, rtp_timestamp);
  }
}

void
RtpH265Depacketizer::handle_single_nal(std::span<const uint8_t> payload)
{
  if (payload.size() < 2) {
    return;
  }

  uint8_t nal_type = extract_nal_type(payload[0]);

  if (is_keyframe_nal(nal_type)) {
    m_frame_has_keyframe = true;
  }

  // Annex-B start code + NAL unit
  append_start_code(m_frame_buffer);
  m_frame_buffer.insert(m_frame_buffer.end(), payload.begin(), payload.end());
}

void
RtpH265Depacketizer::handle_fu(std::span<const uint8_t> payload)
{
  // RFC 7798 Section 4.4.3
  // FU 구조: PayloadHdr(2bytes) + FU header(1byte) + FU payload
  if (payload.size() < 3) {
    return;
  }

  uint8_t fu_header = payload[2];
  bool start = (fu_header & 0x80) != 0;
  bool end = (fu_header & 0x40) != 0;
  uint8_t nal_type = fu_header & 0x3F;

  if (start) {
    // 새 조각화 시작 — Annex-B start code + NAL 헤더를 먼저 기록
    m_accumulator.clear();
    m_in_fragmentation = true;

    // Annex-B 4-byte start code
    static constexpr std::array<uint8_t, 4> kStartCode = {0x00, 0x00, 0x00, 0x01};
    m_accumulator.insert(m_accumulator.end(), kStartCode.begin(), kStartCode.end());

    // NAL 헤더 재구성 (2바이트)
    // byte[0]: F(1) + Type(6) + LayerID_high(1) — Type을 FU header의
    // nal_type으로 교체 byte[1]: LayerID_low(5) + TID(3) — 원본 유지
    uint8_t reconstructed_byte0 = (payload[0] & 0x81) | (nal_type << 1);
    m_accumulator.push_back(reconstructed_byte0);
    m_accumulator.push_back(payload[1]);

    if (is_keyframe_nal(nal_type)) {
      m_frame_has_keyframe = true;
    }
  }

  if (!m_in_fragmentation) {
    return;
  }

  // FU 페이로드 추가 (PayloadHdr(2) + FU header(1) 제외)
  m_accumulator.insert(m_accumulator.end(), payload.begin() + 3, payload.end());

  if (end) {
    // 조각화 완료 -> start code + NAL이 이미 accumulator에 포함됨
    m_in_fragmentation = false;
    m_frame_buffer.insert(
      m_frame_buffer.end(),
      m_accumulator.begin(),
      m_accumulator.end());
    m_accumulator.clear();
  }
}

void
RtpH265Depacketizer::handle_ap(std::span<const uint8_t> payload)
{
  // RFC 7798 Section 4.4.2
  // AP 구조: PayloadHdr(2bytes) + {NALU_size(2bytes) + NAL_data}*

  size_t offset = 2; // PayloadHdr 건너뜀

  while (offset + 2 <= payload.size()) {
    uint16_t nal_size
      = static_cast<uint16_t>((payload[offset] << 8) | payload[offset + 1]);
    offset += 2;

    if (offset + nal_size > payload.size()) {
      spdlog::warn("H.265 AP: NAL 크기({})가 패킷 범위를 초과", nal_size);
      break;
    }

    auto nal_data = payload.subspan(offset, nal_size);

    if (nal_data.size() >= 2) {
      uint8_t nal_type = extract_nal_type(nal_data[0]);
      if (is_keyframe_nal(nal_type)) {
        m_frame_has_keyframe = true;
      }
    }

    // Annex-B start code + NAL unit
    append_start_code(m_frame_buffer);
    m_frame_buffer.insert(m_frame_buffer.end(), nal_data.begin(), nal_data.end());

    offset += nal_size;
  }
}

bool
RtpH265Depacketizer::is_keyframe_nal(uint8_t nal_type)
{
  // IRAP (Intra Random Access Point) NAL 타입
  return nal_type == static_cast<uint8_t>(NalUnitType::kIdrWRadl)
         || nal_type == static_cast<uint8_t>(NalUnitType::kIdrNLp)
         || nal_type == static_cast<uint8_t>(NalUnitType::kCraNut)
         || nal_type == static_cast<uint8_t>(NalUnitType::kBlaWLp)
         || nal_type == static_cast<uint8_t>(NalUnitType::kBlaWRadl)
         || nal_type == static_cast<uint8_t>(NalUnitType::kBlaNLp)
         || nal_type == static_cast<uint8_t>(NalUnitType::kVps)
         || nal_type == static_cast<uint8_t>(NalUnitType::kSps)
         || nal_type == static_cast<uint8_t>(NalUnitType::kPps);
}

uint8_t
RtpH265Depacketizer::extract_nal_type(uint8_t first_byte)
{
  // H.265 NAL 헤더 byte[0]: F(1) | Type(6) | LayerID_high(1)
  return (first_byte >> 1) & 0x3F;
}

void
RtpH265Depacketizer::append_start_code(std::vector<uint8_t>& buffer)
{
  // Annex-B 4-byte start code — constexpr 배열로 insert, 컴파일러가 memcpy로
  // 최적화
  static constexpr std::array<uint8_t, 4> kStartCode = {0x00, 0x00, 0x00, 0x01};
  buffer.insert(buffer.end(), kStartCode.begin(), kStartCode.end());
}

} // namespace nx::rtp
