// 파일: jpeg_parser.cpp
// 생성일: 2026-04-02
// 설명: JPEG 파서 구현 - SOF 마커 파싱, subsampling 자동 감지

#include "jpeg_parser.h"

#include <array>
#include <spdlog/spdlog.h>

namespace nx::media::codec {

namespace {

// JPEG 마커 상수
constexpr uint8_t kMarkerPrefix = 0xFF;
constexpr uint8_t kSOF0 = 0xC0; // Baseline DCT
constexpr uint8_t kSOF1 = 0xC1; // Extended sequential DCT
constexpr uint8_t kSOF2 = 0xC2; // Progressive DCT

/// 마커가 SOF (Start of Frame) 인지 확인
constexpr bool
is_sof_marker(uint8_t marker)
{
  return marker == kSOF0 || marker == kSOF1 || marker == kSOF2;
}

// --- 표준 JPEG Huffman 테이블 (ITU-T Rec. T.81 Annex K) ---

// DC Luma (class=0, id=0)
constexpr uint8_t kDcLumaBits[] = {0, 1, 5, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0, 0, 0};
constexpr uint8_t kDcLumaValues[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

// DC Chroma (class=0, id=1)
constexpr uint8_t kDcChromaBits[] = {0, 3, 1, 1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 0};
constexpr uint8_t kDcChromaValues[] = {0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11};

// AC Luma (class=1, id=0)
constexpr uint8_t kAcLumaBits[] = {0, 2, 1, 3, 3, 2, 4, 3, 5, 5, 4, 4, 0, 0, 1, 0x7D};
constexpr uint8_t kAcLumaValues[] = {
  0x01, 0x02, 0x03, 0x00, 0x04, 0x11, 0x05, 0x12, 0x21, 0x31, 0x41, 0x06, 0x13, 0x51,
  0x61, 0x07, 0x22, 0x71, 0x14, 0x32, 0x81, 0x91, 0xA1, 0x08, 0x23, 0x42, 0xB1, 0xC1,
  0x15, 0x52, 0xD1, 0xF0, 0x24, 0x33, 0x62, 0x72, 0x82, 0x09, 0x0A, 0x16, 0x17, 0x18,
  0x19, 0x1A, 0x25, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x34, 0x35, 0x36, 0x37, 0x38, 0x39,
  0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56, 0x57,
  0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74, 0x75,
  0x76, 0x77, 0x78, 0x79, 0x7A, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89, 0x8A, 0x92,
  0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5, 0xA6, 0xA7,
  0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA, 0xC2, 0xC3,
  0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6, 0xD7, 0xD8,
  0xD9, 0xDA, 0xE1, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF1, 0xF2,
  0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA};

// AC Chroma (class=1, id=1)
constexpr uint8_t kAcChromaBits[]
  = {0, 2, 1, 2, 4, 4, 3, 4, 7, 5, 4, 4, 0, 1, 2, 0x77};
constexpr uint8_t kAcChromaValues[] = {
  0x00, 0x01, 0x02, 0x03, 0x11, 0x04, 0x05, 0x21, 0x31, 0x06, 0x12, 0x41, 0x51, 0x07,
  0x61, 0x71, 0x13, 0x22, 0x32, 0x81, 0x08, 0x14, 0x42, 0x91, 0xA1, 0xB1, 0xC1, 0x09,
  0x23, 0x33, 0x52, 0xF0, 0x15, 0x62, 0x72, 0xD1, 0x0A, 0x16, 0x24, 0x34, 0xE1, 0x25,
  0xF1, 0x17, 0x18, 0x19, 0x1A, 0x26, 0x27, 0x28, 0x29, 0x2A, 0x35, 0x36, 0x37, 0x38,
  0x39, 0x3A, 0x43, 0x44, 0x45, 0x46, 0x47, 0x48, 0x49, 0x4A, 0x53, 0x54, 0x55, 0x56,
  0x57, 0x58, 0x59, 0x5A, 0x63, 0x64, 0x65, 0x66, 0x67, 0x68, 0x69, 0x6A, 0x73, 0x74,
  0x75, 0x76, 0x77, 0x78, 0x79, 0x7A, 0x82, 0x83, 0x84, 0x85, 0x86, 0x87, 0x88, 0x89,
  0x8A, 0x92, 0x93, 0x94, 0x95, 0x96, 0x97, 0x98, 0x99, 0x9A, 0xA2, 0xA3, 0xA4, 0xA5,
  0xA6, 0xA7, 0xA8, 0xA9, 0xAA, 0xB2, 0xB3, 0xB4, 0xB5, 0xB6, 0xB7, 0xB8, 0xB9, 0xBA,
  0xC2, 0xC3, 0xC4, 0xC5, 0xC6, 0xC7, 0xC8, 0xC9, 0xCA, 0xD2, 0xD3, 0xD4, 0xD5, 0xD6,
  0xD7, 0xD8, 0xD9, 0xDA, 0xE2, 0xE3, 0xE4, 0xE5, 0xE6, 0xE7, 0xE8, 0xE9, 0xEA, 0xF2,
  0xF3, 0xF4, 0xF5, 0xF6, 0xF7, 0xF8, 0xF9, 0xFA};

// --- JPEG 스캔 데이터 전용 비트 리더 (바이트 스터핑 FF 00 처리) ---

struct JpegBitReader
{
  const uint8_t* data;
  size_t data_size;
  size_t byte_pos = 0;
  int bits_left = 0;
  uint8_t current_byte = 0;
  bool error = false;

  int read_bit()
  {
    if (bits_left == 0) {
      if (byte_pos >= data_size) {
        error = true;
        return -1;
      }
      current_byte = data[byte_pos++];
      if (current_byte == 0xFF) {
        if (byte_pos >= data_size || data[byte_pos] != 0x00) {
          error = true;
          return -1;
        }
        byte_pos++; // 스터핑 바이트 건너뛰기
      }
      bits_left = 8;
    }
    bits_left--;
    return (current_byte >> bits_left) & 1;
  }

  int read_bits(int n)
  {
    int value = 0;
    for (int i = 0; i < n; i++) {
      int bit = read_bit();
      if (bit < 0) {
        return -1;
      }
      value = (value << 1) | bit;
    }
    return value;
  }
};

// 표준 Huffman 테이블에서 심볼 하나 디코딩
int
decode_huffman_symbol(
  JpegBitReader& reader, const uint8_t* bits_counts, const uint8_t* values)
{
  uint16_t code = 0;
  int value_idx = 0;
  uint16_t first_code = 0;

  for (int len = 1; len <= 16; ++len) {
    int bit = reader.read_bit();
    if (bit < 0) {
      return -1;
    }
    code = static_cast<uint16_t>((code << 1) | bit);

    int count = bits_counts[len - 1];
    if (
      count > 0 && code >= first_code
      && static_cast<uint16_t>(code - first_code) < static_cast<uint16_t>(count)) {
      return values[value_idx + (code - first_code)];
    }
    value_idx += count;
    first_code = static_cast<uint16_t>((first_code + count) << 1);
  }
  return -1;
}

// 8x8 블록 하나 디코딩 시도 (DC + AC)
bool
try_decode_block(
  JpegBitReader& reader,
  const uint8_t* dc_bits,
  const uint8_t* dc_values,
  const uint8_t* ac_bits,
  const uint8_t* ac_values)
{
  int dc_size = decode_huffman_symbol(reader, dc_bits, dc_values);
  if (dc_size < 0) {
    return false;
  }
  if (dc_size > 0 && reader.read_bits(dc_size) < 0) {
    return false;
  }

  for (int ac_count = 0; ac_count < 63;) {
    int sym = decode_huffman_symbol(reader, ac_bits, ac_values);
    if (sym < 0) {
      return false;
    }
    if (sym == 0x00) {
      break;
    } // EOB

    int run = (sym >> 4) & 0x0F;
    int size = sym & 0x0F;
    ac_count += run + 1;
    if (ac_count > 63) {
      return false;
    }
    if (size > 0 && reader.read_bits(size) < 0) {
      return false;
    }
  }
  return true;
}

// 지정된 Y블록 수로 MCU를 시험 디코딩, 성공한 MCU 수 반환
int
try_decode_mcus(
  std::span<const uint8_t> scan_data, int y_blocks_per_mcu, int max_mcus = 10)
{
  JpegBitReader reader{scan_data.data(), scan_data.size()};
  int mcu_count = 0;

  for (int mcu = 0; mcu < max_mcus && !reader.error; ++mcu) {
    bool ok = true;
    for (int b = 0; b < y_blocks_per_mcu && ok; ++b) {
      ok = try_decode_block(
        reader,
        kDcLumaBits,
        kDcLumaValues,
        kAcLumaBits,
        kAcLumaValues);
    }
    if (ok) {
      ok = try_decode_block(
        reader,
        kDcChromaBits,
        kDcChromaValues,
        kAcChromaBits,
        kAcChromaValues);
    }
    if (ok) {
      ok = try_decode_block(
        reader,
        kDcChromaBits,
        kDcChromaValues,
        kAcChromaBits,
        kAcChromaValues);
    }
    if (!ok) {
      break;
    }
    mcu_count++;
  }
  return mcu_count;
}

} // anonymous namespace

std::optional<JpegFrameInfo>
parse_jpeg_frame_info(std::span<const uint8_t> data)
{
  if (data.size() < 4) {
    return std::nullopt;
  }

  // 마커 단위로 순회하여 SOF를 찾음
  std::size_t pos = 0;
  while (pos + 1 < data.size()) {
    if (data[pos] != kMarkerPrefix) {
      ++pos;
      continue;
    }

    // 연속된 0xFF 패딩 건너뛰기
    while (pos + 1 < data.size() && data[pos + 1] == kMarkerPrefix) {
      ++pos;
    }

    if (pos + 1 >= data.size()) {
      break;
    }

    auto marker = data[pos + 1];
    pos += 2;

    // SOI(0xD8), EOI(0xD9) — 길이 필드 없음
    if (marker == 0xD8 || marker == 0xD9) {
      continue;
    }

    // SOS(0xDA) 이후는 엔트로피 데이터 → 탐색 중단
    if (marker == 0xDA) {
      break;
    }

    // 이후 마커는 모두 [length:2BE] 를 가짐
    if (pos + 2 > data.size()) {
      break;
    }

    auto segment_len = static_cast<std::size_t>((data[pos] << 8) | data[pos + 1]);

    if (is_sof_marker(marker)) {
      // SOF 구조: [length:2] [precision:1] [height:2] [width:2] [components:1]
      if (segment_len < 8 || pos + segment_len > data.size()) {
        break;
      }

      JpegFrameInfo info;
      info.precision = data[pos + 2];
      info.height = static_cast<uint32_t>((data[pos + 3] << 8) | data[pos + 4]);
      info.width = static_cast<uint32_t>((data[pos + 5] << 8) | data[pos + 6]);
      info.num_components = data[pos + 7];

      if (info.width > 0 && info.height > 0) {
        return info;
      }
    }

    pos += segment_len;
  }

  return std::nullopt;
}

JpegSubsampling
detect_jpeg_subsampling(std::span<const uint8_t> scan_data)
{
  // 4:2:0: MCU = Y블록 4개(H=2,V=2) + Cb + Cr = 6블록
  // 4:2:2: MCU = Y블록 2개(H=2,V=1) + Cb + Cr = 4블록
  // 제한 없이 디코딩하여 먼저 실패하는 쪽이 잘못된 assumption
  constexpr int kMaxMcus = 10000;
  int score_420 = try_decode_mcus(scan_data, 4, kMaxMcus);
  int score_422 = try_decode_mcus(scan_data, 2, kMaxMcus);

  spdlog::debug(
    "[JpegParser] subsampling 감지: scan_size={}, "
    "score_420={}, score_422={}",
    scan_data.size(),
    score_420,
    score_422);

  if (score_420 > score_422) {
    return JpegSubsampling::kYuv420;
  }
  return JpegSubsampling::kYuv422;
}

} // namespace nx::media::codec
