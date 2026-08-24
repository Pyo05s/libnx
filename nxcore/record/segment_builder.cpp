// 파일: segment_builder.cpp
// 생성일: 2025-12-01
// 설명: 세그먼트 빌더 클래스 구현

// 파일 I/O: std::ofstream 대신 nx::file::FileStream(Win32 CreateFileW 기반)을
// 사용합니다.
// - MSVC CRT _Xfsopen의 전역 critical section 경합 문제를 회피하기 위함입니다.
// - FileStream은 내부 버퍼가 없으므로 write 시 OS에 직접 전달됩니다.
// - OS 레벨 동기화(FlushFileBuffers/fsync)는 sync_to_disk()에서
//   DataSyncLevel::kFull 조건 시 FileStream::flush()를 통해 수행됩니다.

#include "segment_builder.h"
#include "../util/debug_util.h"
#include "record_error.h"


#include <filesystem>

namespace fs = std::filesystem;

namespace nx {
namespace record {

SegmentBuilder::SegmentBuilder(std::shared_ptr<Options> options)
    : m_options(std::move(options))
    , m_current_sequence(0)
{
  NX_REQUIRE_NON_NULL(m_options, "SegmentBuilder::Options");
}

SegmentBuilder::~SegmentBuilder() { close_current_segment(); }

nx::expected<std::size_t>
SegmentBuilder::write_block(const DataBlock& block)
{
  // 블록 유효성 검사
  if (!block.header) {
    return std::unexpected(make_error_code(RecordErrc::invalid_block));
  }

  if (block.serialized.empty()) {
    return std::unexpected(make_error_code(RecordErrc::empty_block));
  }

  // 블록의 start_timestamp를 기준으로 세그먼트 파일 결정
  mstime_t block_timestamp = block.header->start_timestamp;

  // 현재 열린 파일이 없으면 새 세그먼트 생성
  if (!m_file.is_open()) {
    std::error_code ec = create_new_segment(block_timestamp);
    if (ec) {
      return std::unexpected(ec);
    }
  }
  else {
    // 세그먼트 종료 조건 확인 및 필요시 새 세그먼트로 전환
    std::error_code ec = check_and_rotate_segment(block);
    if (ec) {
      return std::unexpected(ec);
    }
  }

  // 블록 쓰기 전 현재 파일 오프셋 저장
  auto pos = m_file.tell();
  if (!pos) {
    return std::unexpected(make_error_code(RecordErrc::file_write_failed));
  }
  uint64_t block_offset = static_cast<uint64_t>(*pos);

  // 블록 데이터를 파일에 기록
  auto write_result = m_file.write({block.serialized.data(), block.serialized.size()});
  if (!write_result) {
    return std::unexpected(make_error_code(RecordErrc::file_write_failed));
  }

  // 컨텍스트 업데이트
  m_ctx.end_timestamp = block.header->end_timestamp;
  m_ctx.block_count++;
  m_ctx.file_size += block.serialized.size();

  // 평균 bitrate 계산
  double duration_sec =
    static_cast<double>(m_ctx.end_timestamp - m_ctx.start_timestamp) / 1000.0;
  if (duration_sec > 0.0) {
    m_ctx.avg_bitrate_bps = (static_cast<double>(m_ctx.file_size) * 8.0) / duration_sec;
  }

  // 키프레임 블록이면 인덱스 엔트리를 메모리에 추가
  if (block.header->flags & static_cast<uint32_t>(BlockFlags::kHasKeyFrame)) {
    // 블록 내 첫 엔트리에서 entry_type/codec_type 추출
    EntryType entry_type{};
    uint8_t codec_type = 0;

    if (!block.entries.empty()) {
      auto const* first_entry = block.entries[0];
      entry_type = static_cast<EntryType>(first_entry->type);

      if (entry_type == EntryType::kVideo) {
        auto const* ve = reinterpret_cast<VideoBlockEntry const*>(first_entry);
        codec_type = ve->codec_type;
      }
      else if (entry_type == EntryType::kAudio) {
        auto const* ae = reinterpret_cast<AudioBlockEntry const*>(first_entry);
        codec_type = ae->codec_type;
      }
      else if (entry_type == EntryType::kUserdata) {
        auto const* ue = reinterpret_cast<UserBlockEntry const*>(first_entry);
        codec_type = ue->data_type;
      }
    }

    IndexEntry entry;
    entry.magic = IndexEntry::kMagic;
    entry.flags = index_flags::encode(block.header->flags, entry_type, codec_type);
    entry.timestamp = block.header->start_timestamp;
    entry.offset = block_offset;

    m_index_entries.push_back(entry);
  }

  // 디스크에 동기화 (sync_blocks 설정에 따라 배치 처리)
  ++m_unflushed_blocks;
  if (m_unflushed_blocks >= m_options->sync_blocks()) {
    std::error_code sync_ec = sync_to_disk();
    if (sync_ec) {
      return std::unexpected(sync_ec);
    }
    m_unflushed_blocks = 0;
  }

  return block.serialized.size();
}

void
SegmentBuilder::close_current_segment()
{
  if (m_file.is_open()) {
    // 세그먼트 푸터 작성 (항상 작성, 인덱스 개수 0 포함)
    write_segment_footer();

    // 파일 닫기 전 최종 동기화
    sync_to_disk();

    m_file.close();

    // 세그먼트 완료 콜백 호출 (경로, 컨텍스트, 인덱스 전달)
    if (!m_current_path.empty()) {
      try {
        m_options->on_segment_finished(m_current_path, m_ctx, std::move(m_index_entries));
      }
      catch (...) {
        // 콜백에서 발생한 예외는 무시
      }
    }
  }

  // 상태 초기화
  m_current_path.clear();
  m_current_sequence = 0;
  m_index_entries.clear();
  m_ctx.clear();
  m_unflushed_blocks = 0;
}

std::string
SegmentBuilder::get_current_segment_path() const
{
  return m_current_path;
}

const SegmentBuilder::Context&
SegmentBuilder::context() const
{
  return m_ctx;
}

std::error_code
SegmentBuilder::create_new_segment(mstime_t utc_timestamp)
{
  // Options를 통해 시퀀스 번호 계산
  int sequence = m_options->get_next_sequence(m_ctx);

  // 세그먼트 파일 경로 생성 (시퀀스 포함)
  std::string path = m_options->make_segment_path(utc_timestamp, sequence);
  if (path.empty()) {
    return make_error_code(RecordErrc::file_open_failed);
  }

  // 파일 존재 확인 및 충돌 알림
  std::error_code ec;
  if (fs::exists(path, ec)) {
    std::string original_path = path; // 충돌 파일 저장

    // 시퀀스 증가하여 중복 회피
    while (fs::exists(path, ec) && sequence < 999) {
      ++sequence;
      path = m_options->make_segment_path(utc_timestamp, sequence);
    }

    // 파일 충돌 콜백 호출
    try {
      m_options->on_segment_file_conflict(
        original_path, // 충돌한 파일
        utc_timestamp, // 생성하려던 시간
        sequence);     // 사용할 시퀀스
    }
    catch (...) {
      // 콜백에서 발생한 예외는 무시
    }
  }

  // 기존 세그먼트가 열려있으면 먼저 닫기 (자동으로 on_segment_finished 콜백 호출)
  if (m_file.is_open()) {
    close_current_segment();
  }

  // 디렉토리 생성 (존재하지 않는 경우)
  fs::path file_path(path);
  fs::path dir_path = file_path.parent_path();
  if (!dir_path.empty()) {
    fs::create_directories(dir_path, ec);
    if (ec) {
      return make_error_code(RecordErrc::directory_create_failed);
    }
  }

  // 파일 열기 (이진 모드, 쓰기 전용)
  auto open_ec = m_file.open(fs::path(path), nx::file::OpenMode::kWriteTruncate);
  if (open_ec) {
    return make_error_code(RecordErrc::file_open_failed);
  }

  // 세그먼트 헤더 기록
  ec = write_segment_header();
  if (ec) {
    close_current_segment();
    return ec;
  }

  // 헤더 기록 후 디스크에 동기화
  ec = sync_to_disk();
  if (ec) {
    close_current_segment();
    return ec;
  }

  // 컨텍스트 초기화
  m_current_path = path;
  m_current_sequence = sequence;
  m_ctx.clear();
  m_ctx.start_timestamp = utc_timestamp;
  m_ctx.end_timestamp = utc_timestamp;
  m_ctx.file_size = sizeof(SegmentHeader); // 헤더 크기로 초기화

  // 인덱스 메모리 버퍼 초기화
  m_index_entries.clear();

  return std::error_code{};
}

std::error_code
SegmentBuilder::write_segment_header()
{
  // 세그먼트 헤더 생성
  SegmentHeader header = m_options->create_segment_header();

  // 헤더를 파일에 기록
  if (auto ec = m_file.write_struct(header); ec) {
    return make_error_code(RecordErrc::header_write_failed);
  }

  return std::error_code{};
}

std::error_code
SegmentBuilder::check_and_rotate_segment(const DataBlock& next_block)
{
  if (!next_block.header) {
    return make_error_code(RecordErrc::invalid_block);
  }

  // Options로부터 세그먼트 종료 여부 확인
  // 다음 블록을 추가했을 때의 상태를 시뮬레이션하여 확인
  Context simulated_ctx = m_ctx;
  simulated_ctx.end_timestamp = next_block.header->end_timestamp;
  simulated_ctx.block_count++;
  simulated_ctx.file_size += next_block.serialized.size();

  if (m_options->should_finish_segment(simulated_ctx)) {
    // 현재 세그먼트 종료
    close_current_segment();

    // 새 세그먼트 시작 (다음 블록의 타임스탬프 기준)
    std::error_code ec = create_new_segment(next_block.header->start_timestamp);
    if (ec) {
      return make_error_code(RecordErrc::segment_rotation_failed);
    }
  }

  return std::error_code{};
}

std::error_code
SegmentBuilder::flush_pending()
{
  if (m_unflushed_blocks == 0) {
    return std::error_code{};
  }

  std::error_code ec = sync_to_disk();
  if (!ec) {
    m_unflushed_blocks = 0;
  }
  return ec;
}

std::error_code
SegmentBuilder::sync_to_disk()
{
  if (!m_file.is_open()) {
    return make_error_code(RecordErrc::segment_not_open);
  }

  // 동기화 레벨 확인
  DataSyncLevel sync_level = m_options->data_sync_level();

  // FileStream은 내부 버퍼가 없으므로 kNormal까지는 별도 flush 불필요
  // kFull: FlushFileBuffers(Windows) / fsync(POSIX) 까지 수행
  bool stream_flushed = false;
  bool os_synced = false;

  if (sync_level == DataSyncLevel::kNone) {
    m_options->on_sync_performed(sync_level, stream_flushed, os_synced);
    return std::error_code{};
  }

  if (sync_level == DataSyncLevel::kFull) {
    auto ec = m_file.flush();
    if (ec) {
      m_options->on_sync_performed(sync_level, stream_flushed, os_synced);
      return make_error_code(RecordErrc::file_sync_failed);
    }
    stream_flushed = true;
    os_synced = true;
  }
  else {
    // kNormal: FileStream은 write 시 OS에 직접 전달되므로 추가 동작 불필요
    stream_flushed = true;
  }

  m_options->on_sync_performed(sync_level, stream_flushed, os_synced);
  return std::error_code{};
}

std::error_code
SegmentBuilder::write_segment_footer()
{
  if (!m_file.is_open()) {
    return make_error_code(RecordErrc::segment_not_open);
  }

  // 인덱스 엔트리가 있는 경우 세그먼트 파일에 기록
  if (!m_index_entries.empty()) {
    // 인덱스 엔트리들을 세그먼트 파일에 기록
    for (const auto& entry : m_index_entries) {
      if (auto ec = m_file.write_struct(entry); ec) {
        return make_error_code(RecordErrc::footer_write_failed);
      }
    }

    // 파일 크기 업데이트
    m_ctx.file_size += sizeof(IndexEntry) * m_index_entries.size();
  }

  // 푸터 헤더 생성 및 기록 (인덱스 개수 0 포함)
  FooterHeader footer;
  footer.magic = FooterHeader::kMagicStart;
  footer.header_size = static_cast<uint16_t>(sizeof(FooterHeader));
  footer.index_count = static_cast<uint32_t>(m_index_entries.size());
  footer.index_size = static_cast<uint32_t>(sizeof(IndexEntry) * m_index_entries.size());
  footer.reserved[0] = 0;
  footer.reserved[1] = 0;
  footer.magic_end = FooterHeader::kMagicEnd;

  if (auto ec = m_file.write_struct(footer); ec) {
    return make_error_code(RecordErrc::footer_write_failed);
  }

  // 파일 크기 업데이트
  m_ctx.file_size += sizeof(FooterHeader);

  return std::error_code{};
}

std::vector<IndexEntry>
SegmentBuilder::get_pending_indices() const
{
  return m_index_entries;
}

} // namespace record
} // namespace nx