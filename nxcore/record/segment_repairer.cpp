// 파일: segment_repairer.cpp
// 생성일: 2026-04-08
// 설명: 미완성 세그먼트 파일 복구 구현

#include "segment_repairer.h"

#include "../file/file_stream.h"

#include <cstring>

namespace fs = std::filesystem;

namespace nx {
namespace record {

namespace {

// 파일 크기 조회
nx::expected<std::uintmax_t>
get_file_size(const fs::path& file_path)
{
  std::error_code ec;
  auto size = fs::file_size(file_path, ec);
  if (ec) {
    return std::unexpected(make_error_code(RecordErrc::file_open_failed));
  }
  return size;
}

// 파일 truncate (지정 크기로 줄임)
std::error_code
truncate_file(const fs::path& file_path, std::uintmax_t size)
{
#ifdef _WIN32
  HANDLE hFile = CreateFileW(
    file_path.c_str(), GENERIC_WRITE, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
    FILE_ATTRIBUTE_NORMAL, nullptr);

  if (hFile == INVALID_HANDLE_VALUE) {
    return make_error_code(RecordErrc::file_open_failed);
  }

  LARGE_INTEGER li;
  li.QuadPart = static_cast<LONGLONG>(size);
  if (!SetFilePointerEx(hFile, li, nullptr, FILE_BEGIN)) {
    CloseHandle(hFile);
    return make_error_code(RecordErrc::file_write_failed);
  }
  if (!SetEndOfFile(hFile)) {
    CloseHandle(hFile);
    return make_error_code(RecordErrc::file_write_failed);
  }
  FlushFileBuffers(hFile);
  CloseHandle(hFile);
#else
  if (::truncate(file_path.c_str(), static_cast<off_t>(size)) != 0) {
    return make_error_code(RecordErrc::file_write_failed);
  }
#endif
  return {};
}

// 구조체를 파일 스트림에서 읽기
// 블록 내 첫 엔트리에서 entry_type/codec_type 추출
void
extract_entry_info(
  nx::file::FileStream& file,
  uint32_t,
  int64_t block_data_start,
  int64_t block_data_end,
  EntryType& out_entry_type,
  uint8_t& out_codec_type)
{
  out_entry_type = EntryType{};
  out_codec_type = 0;

  // 블록에 충분한 데이터가 있는지 확인
  auto available = block_data_end - block_data_start;
  if (available < static_cast<int64_t>(sizeof(BlockEntry))) {
    return;
  }

  auto saved = file.tell();
  if (!saved) {
    return;
  }
  static_cast<void>(file.seek(block_data_start, nx::file::SeekOrigin::kBegin));

  BlockEntry base_entry{};
  if (!file.read_struct(base_entry)) {
    static_cast<void>(file.seek(*saved, nx::file::SeekOrigin::kBegin));
    return;
  }

  out_entry_type = static_cast<EntryType>(base_entry.type);

  // 파생 엔트리의 codec_type 읽기 (header_size가 충분한 경우)
  if (out_entry_type == EntryType::kVideo &&
      base_entry.header_size >= sizeof(VideoBlockEntry)) {
    uint8_t codec = 0;
    auto rd = file.read({&codec, 1});
    if (rd.has_value() && *rd == 1) {
      out_codec_type = codec;
    }
  }
  else if (out_entry_type == EntryType::kAudio &&
           base_entry.header_size >= sizeof(AudioBlockEntry)) {
    uint8_t codec = 0;
    auto rd = file.read({&codec, 1});
    if (rd.has_value() && *rd == 1) {
      out_codec_type = codec;
    }
  }
  else if (out_entry_type == EntryType::kUserdata &&
           base_entry.header_size >= sizeof(UserBlockEntry)) {
    uint8_t data_type = 0;
    auto rd = file.read({&data_type, 1});
    if (rd.has_value() && *rd == 1) {
      out_codec_type = data_type;
    }
  }

  static_cast<void>(file.seek(*saved, nx::file::SeekOrigin::kBegin));
}

} // namespace

// ============================================================================
// check_footer
// ============================================================================

nx::expected<FooterStatus>
SegmentRepairer::check_footer(const fs::path& file_path)
{
  auto size_result = get_file_size(file_path);
  if (!size_result) {
    return std::unexpected(size_result.error());
  }

  auto file_size = *size_result;

  // 최소 크기: SegmentHeader + FooterHeader
  if (file_size < sizeof(SegmentHeader) + sizeof(FooterHeader)) {
    return FooterStatus::kMissing;
  }

  nx::file::FileStream file;
  if (auto ec = file.open(file_path, nx::file::OpenMode::kRead); ec) {
    return std::unexpected(make_error_code(RecordErrc::file_open_failed));
  }

  // 파일 끝에서 FooterHeader 읽기
  auto seek_result =
    file.seek(-static_cast<int64_t>(sizeof(FooterHeader)), nx::file::SeekOrigin::kEnd);
  if (!seek_result) {
    return FooterStatus::kMissing;
  }

  FooterHeader footer{};
  if (!file.read_struct(footer)) {
    return FooterStatus::kMissing;
  }

  // 매직 넘버 검증
  if (footer.magic == FooterHeader::kMagicStart &&
      footer.magic_end == FooterHeader::kMagicEnd) {
    return FooterStatus::kValid;
  }

  // 매직 중 하나만 맞으면 corrupted
  if (footer.magic == FooterHeader::kMagicStart ||
      footer.magic_end == FooterHeader::kMagicEnd) {
    return FooterStatus::kCorrupted;
  }

  return FooterStatus::kMissing;
}

// ============================================================================
// scan_blocks
// ============================================================================

nx::expected<SegmentRepairer::ScanResult>
SegmentRepairer::scan_blocks(const fs::path& file_path)
{
  nx::file::FileStream file;
  if (auto ec = file.open(file_path, nx::file::OpenMode::kRead); ec) {
    return std::unexpected(make_error_code(RecordErrc::file_open_failed));
  }

  // 파일 크기 조회
  auto size_result = file.file_size();
  if (!size_result) {
    return std::unexpected(make_error_code(RecordErrc::file_open_failed));
  }
  auto file_size = static_cast<std::size_t>(*size_result);
  static_cast<void>(file.seek(0, nx::file::SeekOrigin::kBegin));

  // 세그먼트 헤더 읽기
  SegmentHeader seg_header{};
  if (!file.read_struct(seg_header)) {
    return std::unexpected(make_error_code(RecordErrc::file_open_failed));
  }

  if (seg_header.magic != SegmentHeader::kMagic) {
    return std::unexpected(make_error_code(RecordErrc::invalid_block));
  }

  ScanResult result;
  result.channel_id = seg_header.channel_id;

  // 확장 헤더 건너뛰기
  if (seg_header.extension_header_size > 0) {
    auto skip_result = file.seek(
      static_cast<int64_t>(seg_header.extension_header_size),
      nx::file::SeekOrigin::kCurrent);
    if (!skip_result) {
      return std::unexpected(make_error_code(RecordErrc::file_open_failed));
    }
  }

  // 블록 순차 스캔
  while (true) {
    auto tell_result = file.tell();
    if (!tell_result) {
      break;
    }
    auto block_offset = static_cast<std::size_t>(*tell_result);

    // 남은 바이트가 BlockHeader 크기 미만이면 종료
    if (block_offset + sizeof(BlockHeader) > file_size) {
      break;
    }

    BlockHeader block_header{};
    if (!file.read_struct(block_header)) {
      break;
    }

    // 블록 매직 넘버 검증
    if (block_header.magic != BlockHeader::kMagic) {
      break; // 유효하지 않은 블록 → 스캔 중단
    }

    // 블록 길이 유효성 검사
    if (block_header.length < sizeof(BlockHeader) + sizeof(uint16_t)) {
      break; // 최소 크기 미달
    }

    if (block_offset + block_header.length > file_size) {
      break; // 파일 끝을 초과하는 블록 → 불완전
    }

    // 블록 종료 매직 검증
    auto end_magic_offset =
      static_cast<int64_t>(block_offset + block_header.length - sizeof(uint16_t));
    if (!file.seek(end_magic_offset, nx::file::SeekOrigin::kBegin)) {
      break;
    }

    uint16_t end_magic = 0;
    if (!file.read_struct(end_magic)) {
      break;
    }

    if (end_magic != kBlockEndMagic) {
      break; // 종료 매직 불일치 → 불완전 블록
    }

    // 유효한 블록 확인 → 컨텍스트 업데이트
    if (result.block_count == 0) {
      result.start_timestamp = block_header.start_timestamp;
    }
    result.end_timestamp = block_header.end_timestamp;
    result.block_count++;
    result.valid_end_offset = block_offset + block_header.length;

    // 키프레임 블록이면 인덱스 엔트리 생성
    if (block_header.flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame)) {
      // 블록 내 첫 엔트리에서 타입 정보 추출
      auto data_start = static_cast<int64_t>(block_offset + sizeof(BlockHeader));
      auto data_end = end_magic_offset;

      EntryType entry_type{};
      uint8_t codec_type = 0;
      extract_entry_info(
        file, block_header.flags, data_start, data_end, entry_type, codec_type);

      IndexEntry idx;
      idx.magic = IndexEntry::kMagic;
      idx.flags = index_flags::encode(block_header.flags, entry_type, codec_type);
      idx.timestamp = block_header.start_timestamp;
      idx.offset = static_cast<uint64_t>(block_offset);
      result.indices.push_back(idx);
    }

    // 다음 블록 시작 위치로 이동
    static_cast<void>(file.seek(
      static_cast<int64_t>(result.valid_end_offset), nx::file::SeekOrigin::kBegin));
  }

  return result;
}

// ============================================================================
// write_footer_at
// ============================================================================

std::error_code
SegmentRepairer::write_footer_at(
  const fs::path& file_path,
  std::size_t truncate_offset,
  const std::vector<IndexEntry>& indices)
{
  // 파일 크기 조회
  auto size_result = get_file_size(file_path);
  if (!size_result) {
    return size_result.error();
  }

  // 유효 데이터 뒤에 불필요한 바이트가 있으면 truncate
  if (*size_result > truncate_offset) {
    auto ec = truncate_file(file_path, truncate_offset);
    if (ec) {
      return ec;
    }
  }

  // 파일 끝에 append 모드로 열기
  nx::file::FileStream file;
  if (auto ec = file.open(file_path, nx::file::OpenMode::kAppend); ec) {
    return make_error_code(RecordErrc::file_open_failed);
  }

  // 인덱스 엔트리 기록
  for (const auto& entry : indices) {
    if (auto ec = file.write_struct(entry); ec) {
      return make_error_code(RecordErrc::footer_write_failed);
    }
  }

  // 푸터 헤더 기록
  FooterHeader footer;
  footer.magic = FooterHeader::kMagicStart;
  footer.header_size = static_cast<uint16_t>(sizeof(FooterHeader));
  footer.index_count = static_cast<uint32_t>(indices.size());
  footer.index_size = static_cast<uint32_t>(sizeof(IndexEntry) * indices.size());
  footer.reserved[0] = 0;
  footer.reserved[1] = 0;
  footer.magic_end = FooterHeader::kMagicEnd;

  if (auto ec = file.write_struct(footer); ec) {
    return make_error_code(RecordErrc::footer_write_failed);
  }

  // FileStream에 내부 버퍼가 없으므로 별도 flush 불필요
  return {};
}

// ============================================================================
// repair_segment
// ============================================================================

nx::expected<RepairResult>
SegmentRepairer::repair_segment(const fs::path& file_path)
{
  // 1. 이미 footer가 있는지 확인
  auto footer_result = check_footer(file_path);
  if (!footer_result) {
    return std::unexpected(footer_result.error());
  }

  if (*footer_result == FooterStatus::kValid) {
    // 이미 정상 — 복구 불필요
    RepairResult result;
    result.repaired = false;
    return result;
  }

  // 2. 블록 순차 스캔
  auto scan_result = scan_blocks(file_path);
  if (!scan_result) {
    return std::unexpected(scan_result.error());
  }

  auto& scan = *scan_result;

  // 유효 블록이 없으면 복구 불가 (헤더만 존재)
  if (scan.block_count == 0) {
    return std::unexpected(make_error_code(RecordErrc::invalid_block));
  }

  // 3. truncate + footer 기록
  auto ec = write_footer_at(file_path, scan.valid_end_offset, scan.indices);
  if (ec) {
    return std::unexpected(ec);
  }

  // 4. 결과 조립
  RepairResult result;
  result.repaired = true;
  result.channel_id = scan.channel_id;
  result.start_timestamp = scan.start_timestamp;
  result.end_timestamp = scan.end_timestamp;
  result.block_count = scan.block_count;
  result.indices = std::move(scan.indices);

  // 최종 파일 크기 계산
  result.file_size = scan.valid_end_offset + sizeof(IndexEntry) * result.indices.size() +
                     sizeof(FooterHeader);

  // 평균 비트레이트 계산
  double data_size = static_cast<double>(scan.valid_end_offset);
  double duration_sec =
    static_cast<double>(result.end_timestamp - result.start_timestamp) / 1000.0;
  if (duration_sec > 0.0) {
    result.avg_bitrate_bps = (data_size * 8.0) / duration_sec;
  }

  return result;
}

} // namespace record
} // namespace nx
