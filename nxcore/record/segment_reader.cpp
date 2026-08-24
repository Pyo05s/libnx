// 파일: segment_reader.cpp
// 생성일: 2026-02-27
// 설명: 단일 세그먼트 파일 바이너리 파서 구현

#include "segment_reader.h"

#include <nxcore/util/debug_util.h>
#include <nxcore/util/enum_util.h>

#include <spdlog/spdlog.h>

#include <algorithm>
#include <cstring>

namespace nx {
namespace record {

// ============================================================================
// 생성자 / 소멸자
// ============================================================================

SegmentReader::SegmentReader() = default;

SegmentReader::~SegmentReader()
{
  close();
}

// ============================================================================
// 파일 열기 / 닫기
// ============================================================================

std::error_code
SegmentReader::open(const std::filesystem::path& file_path)
{
  if (m_is_open) {
    close();
  }

  // 파일 열기 (존재하지 않으면 kRead 모드에서 에러 반환)
  auto open_ec = m_file.open(file_path, nx::file::OpenMode::kRead);
  if (open_ec) {
    return nx::record::make_error_code(SegmentReaderErrc::kSegmentFileNotFound);
  }

  m_file_path = file_path;

  // 세그먼트 헤더 읽기
  if (!m_file.read_struct(m_segment_header)) {
    close();
    return nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof);
  }

  // 매직 넘버 검증
  if (m_segment_header.magic != SegmentHeader::kMagic) {
    spdlog::error(
      "[SegmentReader] 유효하지 않은 세그먼트 헤더 magic: 0x{:04X}, path={}",
      m_segment_header.magic,
      file_path.string());
    close();
    return nx::record::make_error_code(SegmentReaderErrc::kInvalidSegmentHeader);
  }

  // 확장 헤더 건너뛰기
  if (m_segment_header.extension_header_size > 0) {
    auto seek_result = m_file.seek(
      static_cast<int64_t>(m_segment_header.extension_header_size),
      nx::file::SeekOrigin::kCurrent);
    if (!seek_result) {
      close();
      return nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof);
    }
  }

  m_is_open = true;
  m_need_new_block = true;
  m_entries_read_in_block = 0;
  m_position = {};

  // footer 인덱스 로드 (seek 지원용)
  // footer가 없는 경우(active 세그먼트 등)는 parse_footer()에서 정상 처리
  auto footer_ec = load_footer_index();
  if (footer_ec) {
    spdlog::warn(
      "[SegmentReader] footer 인덱스 로드 실패 (seek 비활성): {}",
      footer_ec.message());
    // footer 로드 실패는 치명적이지 않음 — 순차 읽기는 가능
  }

  return {};
}

void
SegmentReader::close()
{
  if (m_file.is_open()) {
    m_file.close();
  }

  m_is_open = false;
  m_footer_loaded = false;
  m_footer_indices.clear();
  m_segment_header = {};
  m_position = {};
  m_current_block_header = {};
  m_entries_read_in_block = 0;
  m_block_data_end_offset = 0;
  m_need_new_block = true;
}

bool
SegmentReader::is_open() const
{
  return m_is_open;
}

const SegmentHeader&
SegmentReader::segment_header() const
{
  return m_segment_header;
}

// ============================================================================
// 다음 엔트리 읽기
// ============================================================================

nx::expected<ReadFrameResult>
SegmentReader::read_next_entry()
{
  if (!m_is_open) {
    return std::unexpected(nx::record::make_error_code(SegmentReaderErrc::kReadError));
  }

  while (true) {
    // 새 블록 필요 시 블록 헤더 읽기
    if (m_need_new_block) {
      auto block_result = read_block_header();
      if (!block_result) {
        return std::unexpected(block_result.error());
      }

      m_current_block_header = *block_result;
      m_entries_read_in_block = 0;
      m_need_new_block = false;

      // 블록 데이터 종료 위치 계산
      // block_offset + header_size(블록 헤더) + 데이터 영역 = block_offset +
      // length - sizeof(end_magic)
      m_block_data_end_offset
        = m_position.block_offset + m_current_block_header.length
          - static_cast<int64_t>(sizeof(uint16_t)); // end magic 크기
    }

    // 현재 파일 위치 확인 — 블록 데이터 끝에 도달했는지
    auto tell_result = m_file.tell();
    int64_t current_pos = tell_result.has_value() ? *tell_result : 0;
    if (current_pos >= m_block_data_end_offset) {
      // end magic 읽기 및 검증
      uint16_t end_magic = 0;
      [[maybe_unused]]
      bool ok_end = m_file.read_struct(end_magic);

      if (end_magic != kBlockEndMagic) {
        spdlog::warn("[SegmentReader] 블록 종료 매직 불일치: 0x{:04X}", end_magic);
      }

      m_need_new_block = true;
      continue;
    }

    // 블록 내 엔트리 읽기
    auto entry_result = read_entry_from_block(m_current_block_header);
    if (!entry_result) {
      return std::unexpected(entry_result.error());
    }

    m_entries_read_in_block++;
    m_position.entry_index = m_entries_read_in_block;
    m_position.timestamp = entry_result->timestamp;

    return *entry_result;
  }
}

// ============================================================================
// Seek
// ============================================================================

std::error_code
SegmentReader::seek_to_offset(int64_t offset)
{
  if (!m_is_open) {
    return nx::record::make_error_code(SegmentReaderErrc::kReadError);
  }

  auto seek_result = m_file.seek(offset, nx::file::SeekOrigin::kBegin);
  if (!seek_result) {
    return nx::record::make_error_code(SegmentReaderErrc::kSeekFailed);
  }

  m_position.block_offset = offset;
  m_position.entry_index = 0;
  m_entries_read_in_block = 0;
  m_need_new_block = true;

  return {};
}

std::error_code
SegmentReader::seek_to_time(int64_t target_time)
{
  if (!m_is_open) {
    return nx::record::make_error_code(SegmentReaderErrc::kReadError);
  }

  if (!m_footer_loaded || m_footer_indices.empty()) {
    return nx::record::make_error_code(SegmentReaderErrc::kSeekFailed);
  }

  // footer 인덱스에서 이진 탐색: target_time 이전이면서 가장 가까운 블록 찾기
  auto it = std::upper_bound(
    m_footer_indices.begin(),
    m_footer_indices.end(),
    target_time,
    [](int64_t time, const IndexEntry& entry) { return time < entry.timestamp; });

  if (it != m_footer_indices.begin()) {
    --it;
  }

  return seek_to_offset(static_cast<int64_t>(it->offset));
}

// ============================================================================
// 위치 / 시간 범위
// ============================================================================

SegmentPosition
SegmentReader::current_position() const
{
  return m_position;
}

int64_t
SegmentReader::start_time() const
{
  if (!m_footer_loaded || m_footer_indices.empty()) {
    return 0;
  }
  return m_footer_indices.front().timestamp;
}

int64_t
SegmentReader::end_time() const
{
  if (!m_footer_loaded || m_footer_indices.empty()) {
    return 0;
  }
  return m_footer_indices.back().timestamp;
}

// ============================================================================
// Footer 인덱스 로드
// ============================================================================

std::error_code
SegmentReader::load_footer_index()
{
  if (!m_is_open) {
    return nx::record::make_error_code(SegmentReaderErrc::kReadError);
  }

  return parse_footer();
}

std::error_code
SegmentReader::parse_footer()
{
  // 현재 위치 저장
  auto saved_pos = m_file.tell();
  if (!saved_pos) {
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  // 파일 끝에서 FooterHeader 크기만큼 이동하여 읽기
  auto seek_result = m_file.seek(
    -static_cast<int64_t>(sizeof(FooterHeader)),
    nx::file::SeekOrigin::kEnd);
  if (!seek_result) {
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  FooterHeader footer_header{};
  if (!m_file.read_struct(footer_header)) {
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  // 매직 검증 — 유효하지 않으면 footer 미존재로 간주 (active 세그먼트 등)
  if (footer_header.magic != FooterHeader::kMagicStart) {
    spdlog::debug(
      "[SegmentReader] footer 없음 (magic=0x{:04X}), seek 비활성: {}",
      footer_header.magic,
      m_file_path.string());
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return {};
  }

  if (footer_header.index_count == 0) {
    m_footer_indices.clear();
    m_footer_loaded = true;
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return {};
  }

  // 인덱스 엔트리 배열 위치로 이동
  // FooterHeader 직전에 IndexEntry[index_count]가 위치
  auto index_block_size
    = static_cast<int64_t>(footer_header.index_count * sizeof(IndexEntry));
  auto footer_header_size = static_cast<int64_t>(sizeof(FooterHeader));

  auto idx_seek_result
    = m_file.seek(-(footer_header_size + index_block_size), nx::file::SeekOrigin::kEnd);
  if (!idx_seek_result) {
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  // 인덱스 엔트리 읽기
  m_footer_indices.resize(footer_header.index_count);
  auto read_result = m_file.read(
    {reinterpret_cast<uint8_t*>(m_footer_indices.data()),
     static_cast<std::size_t>(index_block_size)});

  if (!read_result || *read_result != static_cast<std::size_t>(index_block_size)) {
    m_footer_indices.clear();
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  // 인덱스 엔트리 매직 검증 (첫 번째만)
  if (!m_footer_indices.empty() && m_footer_indices.front().magic != IndexEntry::kMagic) {
    spdlog::error(
      "[SegmentReader] 유효하지 않은 인덱스 엔트리 magic: 0x{:04X}",
      m_footer_indices.front().magic);
    m_footer_indices.clear();
    static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));
    return nx::record::make_error_code(SegmentReaderErrc::kFooterParseError);
  }

  m_footer_loaded = true;

  // 읽기 위치 복원 (FileStream은 스트림 상태 플래그 없으므로 clear() 불필요)
  static_cast<void>(m_file.seek(*saved_pos, nx::file::SeekOrigin::kBegin));

  return {};
}

// ============================================================================
// 블록 헤더 읽기
// ============================================================================

nx::expected<BlockHeader>
SegmentReader::read_block_header()
{
  auto offset_before_result = m_file.tell();
  int64_t offset_before = offset_before_result.has_value() ? *offset_before_result : 0;

  BlockHeader header{};
  if (!m_file.read_struct(header)) {
    // 읽기 실패 → 예상치 못한 EOF
    return std::unexpected(
      nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
  }

  // 매직 넘버 검증 — footer에 도달하면 블록이 아닌 것으로 판단
  if (header.magic != BlockHeader::kMagic) {
    // footer 영역 또는 파일 끝에 도달한 경우 정상 종료
    return std::unexpected(nx::record::make_error_code(SegmentReaderErrc::kEndOfSegment));
  }

  // 블록 오프셋 기록
  m_position.block_offset = offset_before;

  return header;
}

// ============================================================================
// 엔트리 읽기
// ============================================================================

nx::expected<ReadFrameResult>
SegmentReader::read_entry_from_block(const BlockHeader& /*block_header*/)
{
  // 1. BlockEntry 기본 헤더 읽기
  BlockEntry base_entry{};
  if (!m_file.read_struct(base_entry)) {
    return std::unexpected(
      nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
  }

  // header_size 유효성 검사
  if (base_entry.header_size < sizeof(BlockEntry)) {
    return std::unexpected(
      nx::record::make_error_code(SegmentReaderErrc::kInvalidEntryHeader));
  }

  ReadFrameResult result{};
  result.type = static_cast<EntryType>(base_entry.type);
  result.timestamp = base_entry.timestamp;

  // 2. 파생 타입별 추가 헤더 읽기
  uint32_t payload_size = 0;
  auto remaining_header
    = base_entry.header_size - static_cast<uint16_t>(sizeof(BlockEntry));

  switch (result.type) {
    case EntryType::kVideo: {
      // VideoBlockEntry의 추가 필드 읽기
      VideoBlockEntry video_entry{};
      std::memcpy(&video_entry, &base_entry, sizeof(BlockEntry));

      if (remaining_header > 0) {
        auto* extra_ptr = reinterpret_cast<char*>(&video_entry) + sizeof(BlockEntry);
        auto rd = m_file.read(extra_ptr, remaining_header);
        if (!rd || *rd != remaining_header) {
          return std::unexpected(
            nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
        }
      }

      result.video_codec = static_cast<VideoCodecType>(video_entry.codec_type);
      result.frame_type = static_cast<VideoFrameType>(video_entry.frame_type);
      result.is_keyframe = (result.frame_type == VideoFrameType::kIFrame);
      payload_size = video_entry.payload_size;
      break;
    }

    case EntryType::kAudio: {
      AudioBlockEntry audio_entry{};
      std::memcpy(&audio_entry, &base_entry, sizeof(BlockEntry));

      if (remaining_header > 0) {
        auto* extra_ptr = reinterpret_cast<char*>(&audio_entry) + sizeof(BlockEntry);
        auto rd = m_file.read(extra_ptr, remaining_header);
        if (!rd || *rd != remaining_header) {
          return std::unexpected(
            nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
        }
      }

      result.audio_codec = static_cast<AudioCodecType>(audio_entry.codec_type);
      result.sample_rate = audio_entry.sample_rate;
      result.channels = audio_entry.channels;
      payload_size = audio_entry.payload_size;
      break;
    }

    case EntryType::kUserdata: {
      UserBlockEntry user_entry{};
      std::memcpy(&user_entry, &base_entry, sizeof(BlockEntry));

      if (remaining_header > 0) {
        auto* extra_ptr = reinterpret_cast<char*>(&user_entry) + sizeof(BlockEntry);
        auto rd = m_file.read(extra_ptr, remaining_header);
        if (!rd || *rd != remaining_header) {
          return std::unexpected(
            nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
        }
      }

      payload_size = user_entry.payload_size;
      break;
    }

    default:
      // 알 수 없는 타입 — 헤더 나머지 건너뛰기
      if (remaining_header > 0) {
        static_cast<void>(m_file.seek(remaining_header, nx::file::SeekOrigin::kCurrent));
      }
      return std::unexpected(
        nx::record::make_error_code(SegmentReaderErrc::kInvalidEntryHeader));
  }

  // 3. 페이로드 읽기
  if (payload_size > 0) {
    result.payload.resize(payload_size);
    auto rd = m_file.read(result.payload);
    if (!rd || *rd != payload_size) {
      return std::unexpected(
        nx::record::make_error_code(SegmentReaderErrc::kUnexpectedEof));
    }
  }

  return result;
}

} // namespace record
} // namespace nx
