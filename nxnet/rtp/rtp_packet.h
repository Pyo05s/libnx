// 파일: rtp_packet.h
// 생성일: 2026-02-23
// 설명: RTP 패킷 구조 및 파싱

#pragma once

#include "nxnet/rtp/rtp_types.h"
#include "nxnet/rtp/rtp_error.h"

#include <nxcore/util/type_util.h>

#include <expected>
#include <span>
#include <vector>
#include <cstdint>

namespace nx::rtp {

class RtpPacket
{
public:
  // RTP 패킷 파싱 (payload를 내부 vector에 복사 — 테스트/진단용)
  static nx::expected<RtpPacket> parse(std::span<const uint8_t> data);

  // 수신 핫패스용: 경량 헤더 + payload span만 파싱
  // RtpHeaderView는 스칼라만 포함 — 힙 할당/해제 없음
  // out_payload는 data 내부를 가리키는 뷰 — 복사 없음
  static std::error_code parse_header_and_payload(
    std::span<const uint8_t> data,
    RtpHeaderView& out_header,
    std::span<const uint8_t>& out_payload) noexcept;

  // 헤더 접근
  const RtpHeader& header() const noexcept { return m_header; }

  // 페이로드 접근
  std::span<const uint8_t> payload() const noexcept
  {
    return std::span<const uint8_t>(m_payload);
  }

  // 전체 원본 데이터 크기
  size_t total_size() const noexcept { return m_total_size; }

private:
  RtpPacket() = default;

  RtpHeader m_header;
  std::vector<uint8_t> m_payload;
  size_t m_total_size = 0;
};

} // namespace nx::rtp
