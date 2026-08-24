// 파일: bit_util.h
// 생성일: 2026-04-01
// 설명: 비트스트림 읽기 유틸리티 - Exp-Golomb 디코딩, RBSP 에뮬레이션 방지 바이트
// 제거

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nx {

/// 바이트 배열에서 비트 단위로 읽기 위한 리더
/// Exp-Golomb 부호화(H.264/H.265 SPS 등)에 필요한 ue(v), se(v) 디코딩 지원
class BitReader
{
public:
  explicit BitReader(std::span<const uint8_t> data)
      : m_data(data)
  {}

  /// 지정 비트 수만큼 읽기 (MSB first)
  uint32_t read_bits(int count)
  {
    uint32_t result = 0;
    for (int i = 0; i < count; ++i) {
      if (m_byte_offset >= m_data.size()) {
        return result;
      }
      result <<= 1;
      result |= (m_data[m_byte_offset] >> (7 - m_bit_offset)) & 1;
      advance_bit();
    }
    return result;
  }

  /// 1비트 읽기
  uint32_t read_bit() { return read_bits(1); }

  /// Exp-Golomb 부호 없는 정수 (ue(v))
  uint32_t read_ue()
  {
    int leading_zeros = 0;
    while (read_bit() == 0 && leading_zeros < 32) {
      ++leading_zeros;
    }
    if (leading_zeros == 0) {
      return 0;
    }
    uint32_t value = read_bits(leading_zeros);
    return (1u << leading_zeros) - 1 + value;
  }

  /// Exp-Golomb 부호 있는 정수 (se(v))
  int32_t read_se()
  {
    uint32_t code = read_ue();
    if (code % 2 == 0) {
      return -static_cast<int32_t>(code / 2);
    }
    return static_cast<int32_t>((code + 1) / 2);
  }

  /// 특정 비트 수만큼 건너뛰기
  void skip_bits(int count)
  {
    for (int i = 0; i < count; ++i) {
      advance_bit();
    }
  }

  /// 더 읽을 데이터가 있는지 확인
  bool has_more_data() const { return m_byte_offset < m_data.size(); }

private:
  void advance_bit()
  {
    ++m_bit_offset;
    if (m_bit_offset == 8) {
      m_bit_offset = 0;
      ++m_byte_offset;
    }
  }

  std::span<const uint8_t> m_data;
  std::size_t m_byte_offset = 0;
  int m_bit_offset = 0;
};

} // namespace nx
