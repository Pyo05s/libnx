// 파일: jpeg_parser.h
// 생성일: 2026-04-02
// 설명: JPEG 파서 - SOF 마커 파싱, subsampling 자동 감지

#pragma once

#include <cstdint>
#include <optional>
#include <span>

namespace nx::media::codec {

/// JPEG SOF에서 추출한 이미지 파라미터
struct JpegFrameInfo
{
  uint32_t width = 0;
  uint32_t height = 0;
  uint8_t precision = 0;      // 샘플 비트 수 (보통 8)
  uint8_t num_components = 0; // 컴포넌트 수 (1=그레이, 3=YCbCr)
};

/// JPEG 크로마 서브샘플링 종류
enum class JpegSubsampling : uint8_t
{
  kYuv420, // 4:2:0 - Y블록 4개 + Cb + Cr per MCU
  kYuv422, // 4:2:2 - Y블록 2개 + Cb + Cr per MCU
};

/// JPEG 데이터에서 SOF0/SOF2 마커를 검색하여 프레임 정보 추출
/// @param data JPEG 프레임 (FFD8부터 또는 raw payload)
/// @return 파싱 성공 시 프레임 정보, 실패 시 nullopt
std::optional<JpegFrameInfo> parse_jpeg_frame_info(std::span<const uint8_t> data);

/// JPEG 스캔 데이터에서 실제 subsampling 형식을 감지
/// 표준 Huffman 테이블로 MCU를 시험 디코딩하여 4:2:0 vs 4:2:2 판별
/// @param scan_data SOS 이후의 raw 엔트로피 데이터
/// @return 감지된 subsampling 형식
JpegSubsampling detect_jpeg_subsampling(std::span<const uint8_t> scan_data);

} // namespace nx::media::codec
