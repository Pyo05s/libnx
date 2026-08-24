// 파일: rtp_h264_depacketizer.cpp
// 생성일: 2026-02-23
// 설명: H.264 RTP 디패킷타이저 구현 (RFC 6184)

#include "rtp_h264_depacketizer.h"

#include <spdlog/spdlog.h>

namespace nx::rtp {

RtpH264Depacketizer::RtpH264Depacketizer()
{
  m_accumulator.reserve(256 * 1024);  // 256KB 예약
  m_frame_buffer.reserve(512 * 1024); // 512KB 예약
}

bool
RtpH264Depacketizer::process_packet(
  const RtpHeaderView& header,
  std::span<const uint8_t> payload,
  std::vector<uint8_t>& out_frame,
  bool& out_keyframe)
{
  if (payload.empty()) {
    return false;
  }

  // 타임스탬프 변경 감지 -> 이전 프레임 출력
  if (
    m_last_timestamp != 0 && header.timestamp != m_last_timestamp
    && !m_frame_buffer.empty()) {
    // 이전 프레임 출력 (swap: m_frame_buffer가 out_frame의 capacity를 보존)
    out_frame.swap(m_frame_buffer);
    out_keyframe = m_frame_has_keyframe;
    m_frame_buffer.clear();
    m_frame_has_keyframe = false;

    // 현재 패킷도 새 프레임에 추가 (데이터 손실 방지)
    m_last_timestamp = header.timestamp;
    dispatch_nal(payload, header.timestamp);
    m_last_seq = header.sequence_number;

    // marker 비트가 함께 설정되면 현재 프레임도 pending 큐에 보관
    if (header.marker && !m_frame_buffer.empty()) {
      m_pending_frame.swap(m_frame_buffer);
      m_pending_keyframe = m_frame_has_keyframe;
      m_frame_buffer.clear();
      m_frame_has_keyframe = false;
      m_has_pending = true;
    }

    return true; // 이전 프레임 반환
  }

  // pending 프레임 우선 처리 (타임스탬프 변경 + marker 동시 발생 후)
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

  // marker 비트가 설정된 경우 프레임 완료 (swap: m_frame_buffer가 out_frame의
  // capacity를 보존)
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
RtpH264Depacketizer::reset()
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
RtpH264Depacketizer::dispatch_nal(
  std::span<const uint8_t> payload, uint32_t rtp_timestamp)
{
  if (payload.empty()) {
    return;
  }

  uint8_t nal_type = payload[0] & 0x1F;

  if (nal_type >= 1 && nal_type <= 23) {
    handle_single_nal(payload);
  }
  else if (nal_type == static_cast<uint8_t>(NalUnitType::kFuA)) {
    handle_fu_a(payload);
  }
  else if (nal_type == static_cast<uint8_t>(NalUnitType::kStapA)) {
    handle_stap_a(payload);
  }
  else {
    spdlog::warn("H.264 알 수 없는 NAL 타입: {}", nal_type);
    report_unknown_nal(nal_type, rtp_timestamp);
  }
}

void
RtpH264Depacketizer::handle_single_nal(std::span<const uint8_t> payload)
{
  uint8_t nal_type = payload[0] & 0x1F;

  if (is_keyframe_nal(nal_type)) {
    m_frame_has_keyframe = true;
  }

  // Annex-B start code + NAL
  append_start_code(m_frame_buffer);
  m_frame_buffer.insert(m_frame_buffer.end(), payload.begin(), payload.end());
}

void
RtpH264Depacketizer::handle_fu_a(std::span<const uint8_t> payload)
{
  if (payload.size() < 2) {
    return;
  }

  uint8_t fu_indicator = payload[0];
  uint8_t fu_header = payload[1];

  bool start = (fu_header & 0x80) != 0;
  bool end = (fu_header & 0x40) != 0;
  uint8_t nal_type = fu_header & 0x1F;

  if (start) {
    // 새 조각화 시작 — Annex-B start code + NAL 헤더를 먼저 기록
    // end 시점에 append_start_code 호출 없이 accumulator를 frame_buffer에 바로
    // append
    m_accumulator.clear();
    m_in_fragmentation = true;

    // Annex-B 4-byte start code
    static constexpr std::array<uint8_t, 4> kStartCode = {0x00, 0x00, 0x00, 0x01};
    m_accumulator.insert(m_accumulator.end(), kStartCode.begin(), kStartCode.end());

    // NAL 헤더 재구성: F(1) + NRI(2) from FU indicator + Type(5) from FU header
    uint8_t nal_header = (fu_indicator & 0xE0) | nal_type;
    m_accumulator.push_back(nal_header);

    if (is_keyframe_nal(nal_type)) {
      m_frame_has_keyframe = true;
    }
  }

  if (!m_in_fragmentation) {
    return;
  }

  // 페이로드 추가 (FU indicator + FU header 제외)
  m_accumulator.insert(m_accumulator.end(), payload.begin() + 2, payload.end());

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
RtpH264Depacketizer::handle_stap_a(std::span<const uint8_t> payload)
{
  // STAP-A: 여러 NAL을 하나의 RTP 패킷에 포함
  // 형식: STAP-A header(1byte) + {size(2bytes) + NAL}*

  size_t offset = 1; // STAP-A 헤더 건너뜀

  while (offset + 2 <= payload.size()) {
    uint16_t nal_size
      = static_cast<uint16_t>((payload[offset] << 8) | payload[offset + 1]);
    offset += 2;

    if (offset + nal_size > payload.size()) {
      spdlog::warn("STAP-A: NAL 크기가 패킷 범위를 초과");
      break;
    }

    auto nal_data = payload.subspan(offset, nal_size);
    uint8_t nal_type = nal_data[0] & 0x1F;

    if (is_keyframe_nal(nal_type)) {
      m_frame_has_keyframe = true;
    }

    append_start_code(m_frame_buffer);
    m_frame_buffer.insert(m_frame_buffer.end(), nal_data.begin(), nal_data.end());

    offset += nal_size;
  }
}

bool
RtpH264Depacketizer::is_keyframe_nal(uint8_t nal_type)
{
  // IDR 슬라이스만 키프레임으로 판정
  // SPS/PPS는 파라미터셋이므로 제외: 파라미터셋만 담긴 프레임이 키프레임으로
  // 오인되면 WebCodecs가 실제 IDR 없이 keyframe 동기화를 완료했다고 착각하여
  // 이후 delta 프레임 디코딩 시 "key frame required" 오류 발생
  return nal_type == static_cast<uint8_t>(NalUnitType::kSliceIdr);
}

void
RtpH264Depacketizer::append_start_code(std::vector<uint8_t>& buffer)
{
  // Annex-B 4-byte start code — constexpr 배열로 insert, 컴파일러가 memcpy로
  // 최적화
  static constexpr std::array<uint8_t, 4> kStartCode = {0x00, 0x00, 0x00, 0x01};
  buffer.insert(buffer.end(), kStartCode.begin(), kStartCode.end());
}

} // namespace nx::rtp
