// 파일: rtp_jpeg_depacketizer.cpp
// 생성일: 2026-04-02
// 설명: MJPEG RTP 디패킷타이저 구현 (RFC 2435)

#include "rtp_jpeg_depacketizer.h"

#include "nxmedia/codec/jpeg_parser.h"

#include <algorithm>
#include <spdlog/spdlog.h>

namespace nx::rtp {

namespace {

// RFC 2435 Appendix - JPEG 기본 양자화 테이블 (JPEG DQT 세그먼트용 zigzag 순서)
// ITU-T Rec. T.81 Annex K natural 순서 테이블을 zigzag 스캔 순서로 재배열
// natural 순서 → zigzag 순서 변환:
//   zz[0]=T[0][0], zz[1]=T[0][1], zz[2]=T[1][0], zz[3]=T[2][0], zz[4]=T[1][1], ...
constexpr std::array<uint8_t, 64> kDefaultLumaQuantTable
  = {16, 11, 12,  14,  12,  10, 16, 14,  13,  14,  18,  17,  16, 19,  24,  40,
     26, 24, 22,  22,  24,  49, 35, 37,  29,  40,  58,  51,  61, 60,  57,  51,
     56, 55, 64,  72,  92,  78, 64, 68,  87,  69,  55,  56,  80, 109, 81,  87,
     95, 98, 103, 104, 103, 62, 77, 113, 121, 112, 100, 120, 92, 101, 103, 99};

constexpr std::array<uint8_t, 64> kDefaultChromaQuantTable
  = {17, 18, 18, 24, 21, 24, 47, 26, 26, 47, 99, 66, 56, 66, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99, 99};

// 표준 JPEG Huffman 테이블 (RFC 2035 Appendix)
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

// Huffman 테이블 하나를 DHT 세그먼트 내에 기록
void
write_huffman_table(
  std::vector<uint8_t>& buf,
  uint8_t class_id, // 상위 4비트: class(0=DC,1=AC), 하위 4비트: table id
  const uint8_t* bits,
  size_t bits_len,
  const uint8_t* values,
  size_t values_len)
{
  buf.push_back(class_id);
  buf.insert(buf.end(), bits, bits + bits_len);
  buf.insert(buf.end(), values, values + values_len);
}

} // anonymous namespace

RtpJpegDepacketizer::RtpJpegDepacketizer()
{
  m_scan_data.reserve(256 * 1024);
}

bool
RtpJpegDepacketizer::process_packet(
  const RtpHeaderView& header,
  std::span<const uint8_t> payload,
  std::vector<uint8_t>& out_frame,
  bool& out_keyframe)
{
  // RFC 2435 최소 헤더 크기: 8 bytes
  if (payload.size() < 8) {
    report_unknown_nal(0, header.timestamp);
    return false;
  }

  // 타임스탬프 변경 감지 → 이전 프레임 폐기 (불완전 프레임)
  if (
    m_last_timestamp != 0 && header.timestamp != m_last_timestamp
    && m_frame_started) {
    // 이전 불완전 프레임 폐기
    m_scan_data.clear();
    m_frame_started = false;
    m_has_restart_header = false;
  }
  m_last_timestamp = header.timestamp;

  // JPEG 헤더 파싱
  JpegHeader jpeg_hdr{};
  if (!parse_jpeg_header(payload, jpeg_hdr)) {
    report_unknown_nal(0, header.timestamp);
    return false;
  }

  // RFC 2435 유효 타입 검증: 0-5 (기본), 64-69 (Restart Marker 변형)
  uint8_t base_type = jpeg_hdr.type & 0x3F;
  if (base_type > 5) {
    spdlog::warn("[RtpJpeg] 알 수 없는 JPEG 타입: {}", jpeg_hdr.type);
    report_unknown_nal(jpeg_hdr.type, header.timestamp);
    return false;
  }

  size_t offset = 8;

  // Restart Marker 헤더 (type 64-127)
  if (jpeg_hdr.type >= 64 && jpeg_hdr.type <= 127) {
    if (payload.size() < offset + 4) {
      return false;
    }
    m_restart_header.restart_interval
      = static_cast<uint16_t>((payload[offset] << 8) | payload[offset + 1]);
    m_restart_header.f_l_restart_count
      = static_cast<uint16_t>((payload[offset + 2] << 8) | payload[offset + 3]);
    m_has_restart_header = true;
    offset += 4;
  }

  // Quantization Table 헤더 (Q >= 128, offset == 0)
  std::span<const uint8_t> quant_data;
  if (jpeg_hdr.q >= 128 && jpeg_hdr.fragment_offset == 0) {
    if (payload.size() < offset + 4) {
      return false;
    }
    QuantHeader qh{};
    qh.mbz = payload[offset];
    qh.precision = payload[offset + 1];
    qh.length
      = static_cast<uint16_t>((payload[offset + 2] << 8) | payload[offset + 3]);
    offset += 4;

    if (qh.length > 0) {
      if (payload.size() < offset + qh.length) {
        return false;
      }
      quant_data = payload.subspan(offset, qh.length);
      offset += qh.length;

      // 양자화 테이블 캐시 (precision 포함)
      m_quant_tables.assign(quant_data.begin(), quant_data.end());
      m_quant_tables_cached = true;
      m_cached_q = jpeg_hdr.q;
      m_cached_precision = qh.precision;
    }
  }

  // 첫 번째 패킷인 경우 (fragment_offset == 0) 메타데이터 저장
  if (jpeg_hdr.fragment_offset == 0) {
    m_scan_data.clear();
    m_current_header = jpeg_hdr;
    m_frame_started = true;
  }

  if (!m_frame_started) {
    // 첫 번째 패킷(offset=0)을 놓친 경우 스킵
    return false;
  }

  // 스캔 데이터 추가 (fragment_offset 기반 삽입)
  if (offset < payload.size()) {
    auto scan_payload = payload.subspan(offset);
    auto end_offset = jpeg_hdr.fragment_offset + scan_payload.size();

    // 버퍼 확장이 필요한 경우 (순서대로 도착 또는 갭 발생)
    if (end_offset > m_scan_data.size()) {
      m_scan_data.resize(end_offset);
    }

    // fragment_offset 위치에 데이터 복사 (순차/비순차/중복 모두 처리)
    std::copy(
      scan_payload.begin(),
      scan_payload.end(),
      m_scan_data.begin() + jpeg_hdr.fragment_offset);
  }

  // marker 비트 → 프레임 완성
  if (header.marker && m_frame_started) {
    // 양자화 테이블 결정
    std::span<const uint8_t> qt;
    if (!quant_data.empty()) {
      qt = quant_data;
    }
    else if (m_quant_tables_cached && m_current_header.q == m_cached_q) {
      qt = m_quant_tables;
    }

    build_jpeg_frame(m_current_header, qt, out_frame);
    out_keyframe = true; // MJPEG은 모든 프레임이 키프레임

    m_scan_data.clear();
    m_frame_started = false;
    m_has_restart_header = false;

    return true;
  }

  return false;
}

void
RtpJpegDepacketizer::reset()
{
  m_scan_data.clear();
  m_quant_tables.clear();
  m_quant_tables_cached = false;
  m_cached_q = 0;
  m_cached_precision = 0;
  m_current_header = {};
  m_restart_header = {};
  m_has_restart_header = false;
  m_frame_started = false;
  m_last_timestamp = 0;
  m_detected_type = -1;
  reset_error_count();
}

bool
RtpJpegDepacketizer::parse_jpeg_header(
  std::span<const uint8_t> payload, JpegHeader& header)
{
  if (payload.size() < 8) {
    return false;
  }

  header.type_specific = payload[0];
  header.fragment_offset
    = static_cast<uint32_t>((payload[1] << 16) | (payload[2] << 8) | payload[3]);
  header.type = payload[4];
  header.q = payload[5];
  header.width = payload[6];
  header.height = payload[7];

  return true;
}

bool
RtpJpegDepacketizer::parse_restart_marker_header(
  std::span<const uint8_t> data, RestartMarkerHeader& header)
{
  if (data.size() < 4) {
    return false;
  }
  header.restart_interval = static_cast<uint16_t>((data[0] << 8) | data[1]);
  header.f_l_restart_count = static_cast<uint16_t>((data[2] << 8) | data[3]);
  return true;
}

void
RtpJpegDepacketizer::build_jpeg_frame(
  const JpegHeader& header,
  std::span<const uint8_t> quant_data,
  std::vector<uint8_t>& out_frame)
{
  uint8_t b0 = m_scan_data.size() > 0 ? m_scan_data[0] : 0;
  uint8_t b1 = m_scan_data.size() > 1 ? m_scan_data[1] : 0;
  // spdlog::debug("[RtpJpeg] 프레임조립: q={}, type={}, {}x{}, "
  //     "scan_size={}, scan=[{:02X},{:02X}], quant={}",
  //     header.q, header.type,
  //     static_cast<uint16_t>(header.width) * 8,
  //     static_cast<uint16_t>(header.height) * 8,
  //     m_scan_data.size(), b0, b1, quant_src);

  // RFC 2435 비준수 카메라 감지: 스캔 데이터가 이미 완성된 JPEG 파일인 경우
  // (SOI 마커 FF D8 으로 시작) → 헤더 재구성 없이 그대로 출력
  if (m_scan_data.size() >= 2 && b0 == 0xFF && b1 == 0xD8) {
    spdlog::debug("[RtpJpeg] 패스스루: size={}", m_scan_data.size());
    out_frame = std::move(m_scan_data);
    return;
  }

  uint16_t width = static_cast<uint16_t>(header.width) * 8;
  uint16_t height = static_cast<uint16_t>(header.height) * 8;

  // width/height가 0인 경우 (> 2040 해상도, 확장 헤더 사용)
  if (width == 0 || height == 0) {
    spdlog::warn(
      "JPEG: 해상도 0 감지 (확장 헤더 미지원), width={}, height={}",
      header.width,
      header.height);
    return;
  }

  // 첫 프레임에서 실제 subsampling 자동 감지
  uint8_t effective_type = header.type;
  uint8_t base_type = header.type & 0x3F;
  if (base_type <= 1) {
    if (m_detected_type < 0) {
      auto subsampling = nx::media::codec::detect_jpeg_subsampling(m_scan_data);
      uint8_t restart_bits = header.type & 0xC0;
      effective_type = (subsampling == nx::media::codec::JpegSubsampling::kYuv422)
                         ? static_cast<uint8_t>(restart_bits | 1)
                         : restart_bits;
      if (effective_type != header.type) {
        spdlog::warn(
          "[RtpJpeg] subsampling 불일치 감지: "
          "reported_type={}, detected={}",
          header.type,
          effective_type);
      }
      m_detected_type = effective_type;
    }
    else {
      effective_type = static_cast<uint8_t>(m_detected_type);
    }
  }

  out_frame.clear();
  out_frame.reserve(m_scan_data.size() + 1024);
  write_soi(out_frame);

  // 2. DQT (양자화 테이블)
  if (!quant_data.empty()) {
    // 스트림에 포함된 양자화 테이블 사용
    // precision 비트: bit0 = table0 16-bit, bit1 = table1 16-bit
    bool luma_16bit = (m_cached_precision & 0x01) != 0;
    bool chroma_16bit = (m_cached_precision & 0x02) != 0;
    size_t luma_table_size = luma_16bit ? 128 : 64;
    size_t chroma_table_size = chroma_16bit ? 128 : 64;

    if (quant_data.size() >= luma_table_size) {
      write_dqt(out_frame, 0, quant_data.subspan(0, luma_table_size), luma_16bit);
    }
    if (quant_data.size() >= luma_table_size + chroma_table_size) {
      write_dqt(
        out_frame,
        1,
        quant_data.subspan(luma_table_size, chroma_table_size),
        chroma_16bit);
    }
    else if (quant_data.size() >= luma_table_size) {
      // Chroma 테이블이 없으면 Luma 테이블 재사용
      write_dqt(out_frame, 1, quant_data.subspan(0, luma_table_size), luma_16bit);
    }
  }
  else {
    // Q factor로 기본 테이블 생성
    std::array<uint8_t, 64> luma_table{};
    std::array<uint8_t, 64> chroma_table{};
    make_default_quant_tables(header.q, luma_table, chroma_table);
    write_dqt(out_frame, 0, luma_table, false);
    write_dqt(out_frame, 1, chroma_table, false);
  }

  // 3. SOF0 (감지된 effective_type 사용)
  write_sof0(out_frame, width, height, effective_type);

  // 4. DHT (Huffman 테이블)
  write_dht(out_frame);

  // 5. DRI (Restart Interval, 필요한 경우)
  if (m_has_restart_header && m_restart_header.restart_interval > 0) {
    write_dri(out_frame, m_restart_header.restart_interval);
  }

  // 6. SOS
  write_sos(out_frame, effective_type);

  // 7. 스캔 데이터 (trailing EOI 제거 후 삽입)
  auto scan_end = m_scan_data.size();
  if (
    scan_end >= 2 && m_scan_data[scan_end - 2] == kMarkerPrefix
    && m_scan_data[scan_end - 1] == kEoi) {
    scan_end -= 2;
  }
  out_frame.insert(
    out_frame.end(),
    m_scan_data.begin(),
    m_scan_data.begin() + scan_end);

  // 8. EOI
  write_eoi(out_frame);
}

void
RtpJpegDepacketizer::write_soi(std::vector<uint8_t>& buf)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kSoi);
}

void
RtpJpegDepacketizer::write_dqt(
  std::vector<uint8_t>& buf,
  uint8_t table_id,
  std::span<const uint8_t> table_data,
  bool precision_16bit)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kDqt);

  uint16_t table_len = static_cast<uint16_t>(table_data.size());
  uint16_t seg_length
    = static_cast<uint16_t>(2 + 1 + table_len); // length(2) + Pq/Tq(1) + data

  buf.push_back(static_cast<uint8_t>(seg_length >> 8));
  buf.push_back(static_cast<uint8_t>(seg_length & 0xFF));

  // Pq(상위 4비트: precision, 0=8bit) | Tq(하위 4비트: table id)
  uint8_t pq_tq
    = static_cast<uint8_t>((precision_16bit ? 0x10 : 0x00) | (table_id & 0x0F));
  buf.push_back(pq_tq);

  buf.insert(buf.end(), table_data.begin(), table_data.end());
}

void
RtpJpegDepacketizer::write_sof0(
  std::vector<uint8_t>& buf, uint16_t width, uint16_t height, uint8_t type)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kSof0);

  // type 0/64: YUV 4:2:0, type 1/65: YUV 4:2:2
  bool is_422 = (type == 1 || type == 65);
  uint8_t num_components = 3;

  uint16_t seg_length = static_cast<uint16_t>(2 + 1 + 2 + 2 + 1 + num_components * 3);
  buf.push_back(static_cast<uint8_t>(seg_length >> 8));
  buf.push_back(static_cast<uint8_t>(seg_length & 0xFF));

  buf.push_back(8); // sample precision (8 bits)

  buf.push_back(static_cast<uint8_t>(height >> 8));
  buf.push_back(static_cast<uint8_t>(height & 0xFF));
  buf.push_back(static_cast<uint8_t>(width >> 8));
  buf.push_back(static_cast<uint8_t>(width & 0xFF));

  buf.push_back(num_components);

  // Y 컴포넌트 (id=1)
  buf.push_back(1); // component id
  if (is_422) {
    buf.push_back(0x21); // H=2, V=1 (4:2:2)
  }
  else {
    buf.push_back(0x22); // H=2, V=2 (4:2:0)
  }
  buf.push_back(0); // quant table 0

  // Cb 컴포넌트 (id=2)
  buf.push_back(2);
  buf.push_back(0x11); // H=1, V=1
  buf.push_back(1);    // quant table 1

  // Cr 컴포넌트 (id=3)
  buf.push_back(3);
  buf.push_back(0x11); // H=1, V=1
  buf.push_back(1);    // quant table 1
}

void
RtpJpegDepacketizer::write_dht(std::vector<uint8_t>& buf)
{
  // 모든 4개의 Huffman 테이블을 하나의 DHT 세그먼트에 포함
  buf.push_back(kMarkerPrefix);
  buf.push_back(kDht);

  // 길이 계산: 각 테이블은 1(class/id) + 16(bits) + values_count
  size_t total_len = 2; // segment length 필드 자체
  total_len += 1 + 16 + sizeof(kDcLumaValues);
  total_len += 1 + 16 + sizeof(kDcChromaValues);
  total_len += 1 + 16 + sizeof(kAcLumaValues);
  total_len += 1 + 16 + sizeof(kAcChromaValues);

  buf.push_back(static_cast<uint8_t>(total_len >> 8));
  buf.push_back(static_cast<uint8_t>(total_len & 0xFF));

  // DC Luma (class=0, id=0)
  write_huffman_table(
    buf,
    0x00,
    kDcLumaBits,
    16,
    kDcLumaValues,
    sizeof(kDcLumaValues));
  // DC Chroma (class=0, id=1)
  write_huffman_table(
    buf,
    0x01,
    kDcChromaBits,
    16,
    kDcChromaValues,
    sizeof(kDcChromaValues));
  // AC Luma (class=1, id=0)
  write_huffman_table(
    buf,
    0x10,
    kAcLumaBits,
    16,
    kAcLumaValues,
    sizeof(kAcLumaValues));
  // AC Chroma (class=1, id=1)
  write_huffman_table(
    buf,
    0x11,
    kAcChromaBits,
    16,
    kAcChromaValues,
    sizeof(kAcChromaValues));
}

void
RtpJpegDepacketizer::write_dri(std::vector<uint8_t>& buf, uint16_t restart_interval)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kDri);
  buf.push_back(0x00);
  buf.push_back(0x04); // segment length = 4
  buf.push_back(static_cast<uint8_t>(restart_interval >> 8));
  buf.push_back(static_cast<uint8_t>(restart_interval & 0xFF));
}

void
RtpJpegDepacketizer::write_sos(std::vector<uint8_t>& buf, uint8_t /*type*/)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kSos);

  uint8_t num_components = 3;
  uint16_t seg_length = static_cast<uint16_t>(2 + 1 + num_components * 2 + 3);

  buf.push_back(static_cast<uint8_t>(seg_length >> 8));
  buf.push_back(static_cast<uint8_t>(seg_length & 0xFF));

  buf.push_back(num_components);

  // Y: DC=0, AC=0
  buf.push_back(1);    // component id
  buf.push_back(0x00); // DC table 0 / AC table 0

  // Cb: DC=1, AC=1
  buf.push_back(2);
  buf.push_back(0x11);

  // Cr: DC=1, AC=1
  buf.push_back(3);
  buf.push_back(0x11);

  // spectral selection 및 successive approximation
  buf.push_back(0x00); // Ss
  buf.push_back(0x3F); // Se
  buf.push_back(0x00); // Ah/Al
}

void
RtpJpegDepacketizer::write_eoi(std::vector<uint8_t>& buf)
{
  buf.push_back(kMarkerPrefix);
  buf.push_back(kEoi);
}

void
RtpJpegDepacketizer::make_default_quant_tables(
  uint8_t q,
  std::array<uint8_t, 64>& luma_table,
  std::array<uint8_t, 64>& chroma_table)
{
  // RFC 2435 Appendix A 기반 양자화 테이블 생성
  // Q factor를 IJG 스케일링 팩터로 변환
  int factor;
  if (q < 1) {
    factor = 5000;
  }
  else if (q < 50) {
    factor = 5000 / q;
  }
  else if (q < 100) {
    factor = 200 - q * 2;
  }
  else {
    factor = 1; // Q=100이면 최소 양자화 (최고 품질)
  }

  for (size_t i = 0; i < 64; ++i) {
    int lq = (kDefaultLumaQuantTable[i] * factor + 50) / 100;
    int cq = (kDefaultChromaQuantTable[i] * factor + 50) / 100;

    luma_table[i] = static_cast<uint8_t>(std::clamp(lq, 1, 255));
    chroma_table[i] = static_cast<uint8_t>(std::clamp(cq, 1, 255));
  }
}

} // namespace nx::rtp
