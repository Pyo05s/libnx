// 파일: common.cpp
// 생성일: 2026-04-01
// 설명: 코덱 공통 유틸리티 구현

#include "nxmedia/codec/common.h"

namespace nx::media::codec {

std::vector<uint8_t>
remove_emulation_prevention(std::span<const uint8_t> data)
{
  std::vector<uint8_t> rbsp;
  rbsp.reserve(data.size());

  for (std::size_t i = 0; i < data.size(); ++i) {
    if (
      i + 2 < data.size() && data[i] == 0x00 && data[i + 1] == 0x00
      && data[i + 2] == 0x03) {
      rbsp.push_back(0x00);
      rbsp.push_back(0x00);
      i += 2; // 0x03 스킵
    }
    else {
      rbsp.push_back(data[i]);
    }
  }

  return rbsp;
}

} // namespace nx::media::codec
