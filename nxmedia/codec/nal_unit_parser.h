// 파일: nal_unit_parser.h
// 생성일: 2026-04-01
// 설명: NAL unit 공통 유틸리티 - start code 탐색, NAL 분리

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nx::media::codec {

/// start code 탐색 실패 시 반환값
constexpr std::size_t npos = static_cast<std::size_t>(-1);

/// NAL unit 정보
struct NalUnit
{
  uint8_t type = 0; // H.264: nal_unit_type (5bit), H.265: nal_unit_type (6bit)
  std::span<const uint8_t> data; // start code 제외한 NAL 바이트
};

/// Annex B 바이트스트림에서 NAL unit 목록을 분리
/// @param data Annex B 형식 바이트스트림 (start code 포함)
/// @return NAL unit 목록 (data는 입력 span의 subview)
std::vector<NalUnit> parse_nal_units(std::span<const uint8_t> data);

/// 3바이트(00 00 01) 또는 4바이트(00 00 00 01) start code 탐색
/// @param data 바이트스트림
/// @param offset 탐색 시작 위치
/// @return start code 시작 위치, 없으면 npos
std::size_t find_start_code(std::span<const uint8_t> data, std::size_t offset = 0);

/// start code 길이 반환 (3 또는 4)
/// @param data start code 시작 위치부터의 바이트스트림
/// @return start code 길이, start code가 아니면 0
std::size_t start_code_length(std::span<const uint8_t> data);

} // namespace nx::media::codec
