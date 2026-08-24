// 파일: rtp_jpeg_depacketizer.h
// 생성일: 2026-04-02
// 설명: MJPEG RTP 디패킷타이저 (RFC 2435)

#pragma once

#include "nxnet/rtp/rtp_depacketizer.h"
#include <array>
#include <vector>
#include <cstdint>

namespace nx::rtp {

class RtpJpegDepacketizer : public RtpDepacketizer
{
public:
  RtpJpegDepacketizer();

  bool process_packet(
    const RtpHeaderView& header,
    std::span<const uint8_t> payload,
    std::vector<uint8_t>& out_frame,
    bool& out_keyframe) override;

  void reset() override;

private:
  // RFC 2435 메인 JPEG 헤더 (8 bytes)
  struct JpegHeader
  {
    uint8_t type_specific = 0;    // Type-specific field
    uint32_t fragment_offset = 0; // 24-bit fragment offset
    uint8_t type = 0;             // JPEG type (0, 1, 64, 65 등)
    uint8_t q = 0;                // Quantization factor (0-255)
    uint8_t width = 0;            // Width / 8
    uint8_t height = 0;           // Height / 8
  };

  // RFC 2435 Restart Marker 헤더 (type 64-127)
  struct RestartMarkerHeader
  {
    uint16_t restart_interval = 0;
    uint16_t f_l_restart_count = 0;
  };

  // RFC 2435 Quantization Table 헤더
  struct QuantHeader
  {
    uint8_t mbz = 0;
    uint8_t precision = 0;
    uint16_t length = 0;
  };

  // JPEG 파일 생성을 위한 마커 상수
  static constexpr uint8_t kMarkerPrefix = 0xFF;
  static constexpr uint8_t kSoi = 0xD8;  // Start Of Image
  static constexpr uint8_t kEoi = 0xD9;  // End Of Image
  static constexpr uint8_t kDqt = 0xDB;  // Define Quantization Table
  static constexpr uint8_t kSof0 = 0xC0; // Start Of Frame (Baseline)
  static constexpr uint8_t kDht = 0xC4;  // Define Huffman Table
  static constexpr uint8_t kSos = 0xDA;  // Start Of Scan
  static constexpr uint8_t kDri = 0xDD;  // Define Restart Interval

  // RTP 페이로드에서 JPEG 헤더 파싱
  static bool parse_jpeg_header(std::span<const uint8_t> payload, JpegHeader& header);

  // Restart Marker 헤더 파싱 (type 64-127)
  static bool parse_restart_marker_header(
    std::span<const uint8_t> data, RestartMarkerHeader& header);

  // 완성된 스캔 데이터로 JPEG 파일 생성
  void build_jpeg_frame(
    const JpegHeader& header,
    std::span<const uint8_t> quant_data,
    std::vector<uint8_t>& out_frame);

  // JPEG 파일 헤더 구성요소 생성
  static void write_soi(std::vector<uint8_t>& buf);
  static void write_dqt(
    std::vector<uint8_t>& buf,
    uint8_t table_id,
    std::span<const uint8_t> table_data,
    bool precision_16bit);
  static void write_sof0(
    std::vector<uint8_t>& buf, uint16_t width, uint16_t height, uint8_t type);
  static void write_dht(std::vector<uint8_t>& buf);
  static void write_dri(std::vector<uint8_t>& buf, uint16_t restart_interval);
  static void write_sos(std::vector<uint8_t>& buf, uint8_t type);
  static void write_eoi(std::vector<uint8_t>& buf);

  // 기본 양자화 테이블 생성 (Q factor 기반)
  static void make_default_quant_tables(
    uint8_t q,
    std::array<uint8_t, 64>& luma_table,
    std::array<uint8_t, 64>& chroma_table);

  // 프레임 조립 버퍼 (fragment_offset 순서로 스캔 데이터 누적)
  std::vector<uint8_t> m_scan_data;

  // 양자화 테이블 캐시 (Q >= 128 스트림에서 첫 프레임의 테이블 저장)
  std::vector<uint8_t> m_quant_tables;
  bool m_quant_tables_cached = false;
  uint8_t m_cached_q = 0;
  uint8_t m_cached_precision = 0; // 양자화 테이블 precision 비트마스크

  // 첫 번째 패킷의 메타데이터 저장
  JpegHeader m_current_header{};
  RestartMarkerHeader m_restart_header{};
  bool m_has_restart_header = false;
  bool m_frame_started = false;

  uint32_t m_last_timestamp = 0;
  int16_t m_detected_type = -1; // 자동 감지된 subsampling type (-1=미감지)
};

} // namespace nx::rtp
