// 파일: block_builder.cpp
// 생성일: 2025-11-24
// 설명: 녹화 파일의 블록과 엔트리를 생성하는 빌더 클래스 구현

#include "block_builder.h"

#include <cstring>
#include <algorithm>

#include <nxcore/util/debug_util.h>
#include <nxcore/util/enum_util.h>

namespace nx {
namespace record {

// 생성자
BlockBuilder::BlockBuilder(
  std::shared_ptr<Options> options, std::size_t pool_max_free)
    : m_options(std::move(options))
    , m_pool(std::make_shared<BlockBufferPool>(pool_max_free))
{
  NX_REQUIRE_NON_NULL(m_options, "BlockBuilder::Options");
  m_ctx.last_activity_time = std::chrono::steady_clock::now();
}

// 소멸자: 남아있는 현재 블록을 최종화하여 블록 목록에 추가
BlockBuilder::~BlockBuilder()
{
  // 남아있는 엔트리가 있다면 블록으로 완성
  finalize_current_block();
}

// 엔트리의 종료 timestamp 계산 (audio의 경우 duration 포함)
mstime_t
BlockBuilder::calculate_entry_end_timestamp(const BlockEntryBuffer* entry_buf) const
{
  if (!entry_buf || !entry_buf->entry)
    return 0;

  int64_t end_timestamp = entry_buf->entry->timestamp;

  if (cmp_enum_equal(entry_buf->entry->type, record::BlockType::kAudio)) {
    // 오디오 엔트리인 경우 데이터의 길이로부터 마지막 시간스탬프 계산
    auto const* audio_entry
      = reinterpret_cast<const AudioBlockEntry*>(entry_buf->entry.get());
    uint32_t sample_rate = audio_entry->sample_rate;
    uint32_t payload_size = audio_entry->payload_size;
    uint32_t bytes_per_sample = (audio_entry->bit_depth / 8) * audio_entry->channels;
    if (sample_rate > 0 && payload_size > 0 && bytes_per_sample > 0) {
      // 오디오 데이터 길이로부터 지속 시간 계산 (밀리초 단위)
      uint32_t total_samples = payload_size / bytes_per_sample;
      int64_t duration_ms
        = (static_cast<int64_t>(total_samples) * 1000) / sample_rate;
      end_timestamp = audio_entry->timestamp + duration_ms;
    }
  }

  return end_timestamp;
}

// 엔트리 추가 (shared_ptr로 소유권 이전)
void
BlockBuilder::add_entry(std::shared_ptr<BlockEntryBuffer> entry_buf)
{
  if (!entry_buf || !entry_buf->entry)
    return;

  // 활동 시간 갱신
  m_ctx.last_activity_time = std::chrono::steady_clock::now();

  // 엔트리 허용 여부 체크 (timestamp 검증 등)
  mstime_t new_entry_timestamp = entry_buf->entry->timestamp;
  if (!m_options->should_accept_entry(m_ctx, new_entry_timestamp)) {
    // 거부된 경우 콜백 호출 후 추가하지 않음
    m_options->on_entry_rejected(new_entry_timestamp, m_ctx.start_timestamp);
    return;
  }

  // 새 엔트리 추가 전 블록 완료 여부 체크 (pending entries가 있는 경우에만)
  if (!m_pending_entries.empty()) {
    mstime_t new_entry_start_ts = entry_buf->entry->timestamp;
    mstime_t new_entry_end_ts = calculate_entry_end_timestamp(entry_buf.get());

    if (
      m_options->should_finalize_before_add(
        m_ctx,
        new_entry_start_ts,
        new_entry_end_ts)) {
      finalize_current_block();
    }
  }

  // 첫 엔트리이면 블록 타임스탬프를 설정
  if (m_pending_entries.empty()) {
    m_ctx.start_timestamp = entry_buf->entry->timestamp;
    m_ctx.end_timestamp = entry_buf->entry->timestamp;
  }

  // 엔트리 종료 timestamp 계산 (헬퍼 함수 재사용)
  int64_t end_timestamp = calculate_entry_end_timestamp(entry_buf.get());

  if (cmp_enum_equal(entry_buf->entry->type, record::BlockType::kVideo)) {
    // 비디오 엔트리인 경우 프레임 타입이 I-프레임이면 키프레임 플래그 설정
    auto const* video_entry
      = reinterpret_cast<const VideoBlockEntry*>(entry_buf->entry.get());
    if (cmp_enum_equal(video_entry->frame_type, record::VideoFrameType::kIFrame)) {
      m_ctx.contains_keyframe = true;
    }
  }
  // else if (cmp_enum_equal(entry_buf->entry->type, record::BlockType::kAudio)) {
  //     // 오디오 엔트리인 경우 특별 처리 없음
  // }
  // else if (cmp_enum_equal(entry_buf->entry->type, record::BlockType::kUserdata))
  // {
  //     // 사용자 정의 데이터 엔트리인 경우 특별 처리 없음
  // }

  if (end_timestamp > m_ctx.end_timestamp) {
    m_ctx.end_timestamp = end_timestamp;
  }

  // update pending size (approx: header + payload)
  m_ctx.pending_bytes
    += entry_buf->entry->header_size
       + static_cast<uint32_t>(entry_buf->payload ? entry_buf->payload->size() : 0);

  m_pending_entries.push_back(std::move(entry_buf));
  m_ctx.pending_count = m_pending_entries.size();

  // 만약 옵션에서 블록이 완료되었다고 하면 현재 블록 최종화
  if (m_options->is_finished(m_ctx)) {
    finalize_current_block();
  }
}

// 현재 블록을 완성하여 m_blocks 에 추가
void
BlockBuilder::finalize_current_block()
{
  if (m_pending_entries.empty()) {
    m_pending_entries.clear();
    m_ctx.clear();
    return;
  }

  // 엔트리들을 timestamp 기준으로 정렬
  std::sort(
    m_pending_entries.begin(),
    m_pending_entries.end(),
    [](auto const& a, auto const& b) {
      return a->entry->timestamp < b->entry->timestamp;
    });

  // 블록 헤더 및 종료 매직 포함 전체 크기 계산
  uint32_t header_size = static_cast<uint32_t>(sizeof(BlockHeader));
  uint32_t end_magic_size = static_cast<uint32_t>(sizeof(uint16_t));

  // 각 엔트리의 직렬화 크기 합산
  uint32_t entries_size = 0;
  for (auto const& eb : m_pending_entries) {
    uint32_t entry_header_size = eb->entry->header_size;
    // 엔트리 헤더 크기 유효성 검사: 0이면 assert 발생
    NX_ASSERT(entry_header_size != 0);
    entries_size += entry_header_size;
    entries_size += static_cast<uint32_t>(eb->payload ? eb->payload->size() : 0);
  }

  uint32_t total_size = header_size + entries_size + end_magic_size;

  // 풀 기반 직렬화 버퍼 획득 (재사용 또는 새 할당)
  DataBlock block{};
  block.serialized = m_pool->acquire(total_size);
  uint8_t* base = block.serialized.data();

  // 블록 헤더 채우기
  BlockHeader hdr;
  hdr.magic = BlockHeader::kMagic;
  hdr.header_size = static_cast<uint16_t>(header_size);
  hdr.flags = 0;

  // 키프레임 포함 시 플래그 설정
  if (m_ctx.contains_keyframe) {
    hdr.flags |= static_cast<uint32_t>(BlockFlags::kHasKeyFrame);
  }

  // Storage Pressure 플래그: 엔트리 중 하나라도 설정됐으면 블록에 반영
  for (const auto& eb : m_pending_entries) {
    if (eb->pressure_flags != 0) {
      hdr.flags |= eb->pressure_flags;
    }
  }

  hdr.length = total_size;
  hdr.start_timestamp = m_ctx.start_timestamp;
  hdr.end_timestamp = m_ctx.end_timestamp;

  // 메모리 채우기
  std::memcpy(base, &hdr, header_size);

  uint32_t write_offset = header_size;
  block.entries.clear();

  for (auto const& eb : m_pending_entries) {
    uint32_t entry_header_size = eb->entry->header_size;

    // 추가 안전 검사: 복사 직전에 헤더 크기가 0인지 다시 확인하여 assert 발생
    NX_ASSERT(entry_header_size != 0);

    // 엔트리 헤더 복사 (shared_ptr가 가리키는 파생 구조체 메모리 시작에서
    // header_size 바이트 복사)
    std::memcpy(base + write_offset, eb->entry.get(), entry_header_size);

    // 엔트리 포인터 등록 (buffer 내의 위치)
    BlockEntry const* eptr = reinterpret_cast<BlockEntry const*>(base + write_offset);
    block.entries.push_back(eptr);

    write_offset += entry_header_size;

    // 페이로드 복사
    if (eb->payload && !eb->payload->empty()) {
      std::memcpy(base + write_offset, eb->payload->data(), eb->payload->size());
      write_offset += static_cast<uint32_t>(eb->payload->size());
    }
  }

  // 종료 매직 복사
  uint16_t end_magic = kBlockEndMagic;
  std::memcpy(base + write_offset, &end_magic, end_magic_size);

  // 종료 매직 포인터 설정
  block.end_magic
    = reinterpret_cast<uint16_t const*>(base + total_size - end_magic_size);
  block.header = reinterpret_cast<BlockHeader const*>(base);

  // 블록 추가
  m_blocks.push_back(std::move(block));

  // pending 초기화
  m_pending_entries.clear();
  m_ctx.clear();
}

// 블록 하나를 꺼내 반환 (큐의 앞쪽)
DataBlock
BlockBuilder::pop_block()
{
  if (m_blocks.empty()) {
    // 빈 블록 반환
    return {};
  }

  DataBlock b = std::move(m_blocks.front());
  m_blocks.erase(m_blocks.begin());
  return b;
}

// 현재 저장된 블록 개수 반환
std::size_t
BlockBuilder::get_block_count() const
{
  return m_blocks.size();
}

} // namespace record
} // namespace nx
