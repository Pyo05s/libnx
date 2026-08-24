// 파일: rtp_header_extension.h
// 생성일: 2026-03-30
// 설명: RFC 5285 One-Byte RTP Header Extension 빌더 및 파서

#pragma once

#include "nxnet/rtp/rtp_types.h"

#include <cstdint>
#include <optional>
#include <span>
#include <vector>

namespace nx::rtp {

/// RFC 5285 One-Byte Header Extension 직렬화/역직렬화
/// - profile 0xBEDE, ID 1~14, 데이터 1~16 bytes
/// - 32-bit 정렬 패딩 자동 처리
class RtpHeaderExtensionBuilder
{
public:
  /// 확장 요소 추가
  /// @param id 확장 ID (1~14, 범위 외 무시)
  /// @param data 확장 데이터 (1~16 bytes, 범위 외 무시)
  void add(uint8_t id, std::span<const uint8_t> data);

  /// NTP-64 타임스탬프 요소 추가 (RFC 6051)
  /// @param id 확장 ID (SDP a=extmap에서 협상된 값)
  /// @param ntp64 RFC 3550 형식 64-bit NTP 타임스탬프
  void add_ntp64(uint8_t id, uint64_t ntp64);

  /// 직렬화된 확장 블록 크기 계산 (4-byte 확장 헤더 포함, 32-bit 정렬)
  /// 요소가 없으면 0 반환
  size_t serialized_size() const;

  /// 확장 블록을 패킷 버퍼에 직렬화
  /// - 프로필 0xBEDE + length + 요소들 + 패딩
  /// - 요소가 없으면 아무것도 쓰지 않음
  void serialize(std::vector<uint8_t>& packet) const;

  /// 등록된 요소 수
  size_t element_count() const { return m_elements.size(); }

  /// 비어 있는지 확인
  bool empty() const { return m_elements.empty(); }

private:
  std::vector<RtpHeaderExtElement> m_elements;
};

/// RFC 5285 One-Byte Header Extension 파싱
/// @param ext_data 확장 블록의 data 부분 (profile/length 이후)
/// @return 파싱된 요소 목록
std::vector<RtpHeaderExtElement>
parse_one_byte_extension(std::span<const uint8_t> ext_data);

/// 파싱된 요소 목록에서 특정 ID의 NTP-64 타임스탬프 추출
/// @param elements 파싱된 확장 요소 목록
/// @param id NTP-64 확장 ID
/// @return NTP-64 타임스탬프 (해당 ID가 없거나 데이터 크기 불일치 시 nullopt)
std::optional<uint64_t>
extract_ntp64(const std::vector<RtpHeaderExtElement>& elements, uint8_t id);

} // namespace nx::rtp
