// 파일: nal_unit_parser.cpp
// 생성일: 2026-04-01
// 설명: NAL unit 공통 유틸리티 - start code 탐색, NAL 분리 구현

#include "nxmedia/codec/nal_unit_parser.h"

namespace nx::media::codec {

std::size_t
find_start_code(std::span<const uint8_t> data, std::size_t offset)
{
  if (data.size() < 3) {
    return npos;
  }

  for (std::size_t i = offset; i + 2 < data.size(); ++i) {
    // 4바이트 start code: 00 00 00 01
    if (
      i + 3 < data.size() && data[i] == 0x00 && data[i + 1] == 0x00
      && data[i + 2] == 0x00 && data[i + 3] == 0x01) {
      return i;
    }
    // 3바이트 start code: 00 00 01
    if (data[i] == 0x00 && data[i + 1] == 0x00 && data[i + 2] == 0x01) {
      return i;
    }
  }

  return npos;
}

std::size_t
start_code_length(std::span<const uint8_t> data)
{
  if (
    data.size() >= 4 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x00
    && data[3] == 0x01) {
    return 4;
  }
  if (data.size() >= 3 && data[0] == 0x00 && data[1] == 0x00 && data[2] == 0x01) {
    return 3;
  }
  return 0;
}

std::vector<NalUnit>
parse_nal_units(std::span<const uint8_t> data)
{
  std::vector<NalUnit> units;

  auto pos = find_start_code(data, 0);
  if (pos == npos) {
    return units;
  }

  while (pos != npos) {
    auto sc_len = start_code_length(data.subspan(pos));
    auto nal_start = pos + sc_len;

    if (nal_start >= data.size()) {
      break;
    }

    // 다음 start code 탐색
    auto next_pos = find_start_code(data, nal_start);
    auto nal_end = (next_pos != npos) ? next_pos : data.size();

    // trailing zero 제거 (다음 start code 앞의 00 바이트)
    while (nal_end > nal_start && data[nal_end - 1] == 0x00) {
      --nal_end;
    }

    if (nal_end > nal_start) {
      NalUnit unit;
      unit.data = data.subspan(nal_start, nal_end - nal_start);
      unit.type = unit.data[0]; // 전체 첫 바이트 (호출자가 코덱별로 마스킹)
      units.push_back(unit);
    }

    pos = next_pos;
  }

  return units;
}

} // namespace nx::media::codec
