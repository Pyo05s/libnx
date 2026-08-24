// 파일: block_builder_unittest.cpp
// 생성일: 2025-11-24
// 설명: BlockBuilder 유닛테스트

#include "nxcore/record/block_builder.h"
#include "nxcore/record/block_builder_options.h"
#include <gtest/gtest.h>
#include <vector>
#include <cstring>

using namespace nx;

namespace {
// TestOptions 구현
class TestOptions : public record::BasicBlockBuilderOptions
{
public:
  TestOptions(bool finished = false)
      : BasicBlockBuilderOptions(nx::seconds(10), 4 * 1024 * 1024, 1000)
      , m_finished(finished)
  {}

  bool is_finished(record::BlockBuilder::Context&) const override { return m_finished; }

  void set_finished(bool v) { m_finished = v; }

private:
  bool m_finished;
};

// 유틸: BlockEntry(T) + payload 를 BlockEntryBuffer 로 래핑
template <typename T>
static std::shared_ptr<record::BlockEntryBuffer>
make_entry_buffer_from(const T& e, const uint8_t* payload, size_t payload_size)
{
  auto buf = std::make_shared<record::BlockEntryBuffer>();
  // make a shared_ptr of the concrete entry type and store as BlockEntry
  auto concrete = std::make_shared<T>(e);
  buf->entry = std::static_pointer_cast<record::BlockEntry>(concrete);

  if (payload && payload_size > 0) {
    buf->payload
      = std::make_shared<std::vector<uint8_t>>(payload, payload + payload_size);
  }
  return buf;
}
} // namespace

TEST(BlockBuilderTest, SingleEntryPayloadSerialized)
{
  auto opts = std::make_shared<TestOptions>(true); // 즉시 블록 완료 트리거
  record::BlockBuilder builder(opts);

  // 비디오 엔트리 준비
  record::VideoBlockEntry ve;
  memset(&ve, 0, sizeof(ve));
  ve.type = static_cast<uint8_t>(record::BlockType::kVideo);
  ve.archive_type = 0;
  ve.header_size = static_cast<uint16_t>(sizeof(record::VideoBlockEntry));
  ve.timestamp = 123456789;
  ve.codec_type = static_cast<uint8_t>(record::VideoCodecType::kH264);
  ve.frame_type = static_cast<uint8_t>(record::VideoFrameType::kIFrame);
  const std::vector<uint8_t> payload = {0x11, 0x22, 0x33, 0x44};
  ve.payload_size = static_cast<uint32_t>(payload.size());

  auto buf = make_entry_buffer_from(ve, payload.data(), payload.size());
  builder.add_entry(buf);

  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block = builder.pop_block();
  ASSERT_FALSE(block.serialized.empty());

  // 헤더 검사
  const record::BlockHeader* hdr = block.header;
  ASSERT_NE(hdr, nullptr);
  EXPECT_EQ(hdr->magic, record::BlockHeader::kMagic);
  EXPECT_EQ(hdr->start_timestamp, ve.timestamp);

  // 엔트리 포인터 및 페이로드 검사
  ASSERT_EQ(block.entries.size(), 1u);
  const record::BlockEntry* eptr = block.entries[0];
  ASSERT_NE(eptr, nullptr);

  // 엔트리 타입이 비디오인지 확인
  EXPECT_EQ(eptr->type, static_cast<uint8_t>(record::BlockType::kVideo));

  // payload 위치와 내용 확인
  uint32_t header_size = static_cast<uint32_t>(sizeof(record::BlockHeader));
  uint32_t entry_header_size = static_cast<uint32_t>(sizeof(record::VideoBlockEntry));
  const uint8_t* base = block.serialized.data();
  const uint8_t* payload_ptr
    = base + header_size + (reinterpret_cast<const uint8_t*>(eptr) - (base + header_size))
      + entry_header_size;
  for (size_t i = 0; i < payload.size(); ++i) {
    EXPECT_EQ(payload_ptr[i], payload[i]);
  }

  // 종료 매직 검사
  uint16_t end_magic = *block.end_magic;
  EXPECT_EQ(end_magic, record::kBlockEndMagic);
}

TEST(BlockBuilderTest, MultipleEntriesFinalizeTrigger)
{
  auto opts = std::make_shared<TestOptions>(false);
  record::BlockBuilder builder(opts);

  // 첫 번째 엔트리 (오디오)
  record::AudioBlockEntry ae{};
  std::memset(&ae, 0, sizeof(ae));
  ae.type = static_cast<uint8_t>(record::BlockType::kAudio);
  ae.header_size = static_cast<uint16_t>(sizeof(record::AudioBlockEntry));
  ae.timestamp = 1000;
  ae.codec_type = static_cast<uint8_t>(record::AudioCodecType::kG711);
  ae.channels = 1;
  ae.bit_depth = 16;
  ae.sample_rate = 8000;
  const std::vector<uint8_t> audio_payload = {0xAA, 0xBB};
  ae.payload_size = static_cast<uint32_t>(audio_payload.size());

  auto b0 = make_entry_buffer_from(ae, audio_payload.data(), audio_payload.size());
  builder.add_entry(b0);

  // 두 번째 엔트리 (userdata)
  record::UserBlockEntry ue{};
  std::memset(&ue, 0, sizeof(ue));
  ue.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  ue.header_size = static_cast<uint16_t>(sizeof(record::UserBlockEntry));
  ue.timestamp = 2000;
  ue.data_type = 0x7F;
  const std::vector<uint8_t> user_payload = {0x01, 0x02, 0x03};
  ue.payload_size = static_cast<uint32_t>(user_payload.size());

  auto b1 = make_entry_buffer_from(ue, user_payload.data(), user_payload.size());
  builder.add_entry(b1);

  // 완료 플래그를 세팅하고 트리거 엔트리를 추가하여 현재 블록을 최종화
  opts->set_finished(true);
  // 트리거용 빈 엔트리 (추가되면 finalize가 호출되어 블록이 완성됩니다)
  record::BlockEntry trigger{};
  std::memset(&trigger, 0, sizeof(trigger));
  trigger.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  trigger.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));
  trigger.timestamp = 3000;
  auto b2 = make_entry_buffer_from(trigger, nullptr, 0);
  builder.add_entry(b2);

  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block = builder.pop_block();
  ASSERT_EQ(block.entries.size(), 3u);

  // 각 페이로드 위치 확인
  uint32_t header_size = static_cast<uint32_t>(sizeof(record::BlockHeader));
  const uint8_t* base = block.serialized.data();

  // 첫 엔트리 payload
  const record::BlockEntry* e0 = block.entries[0];
  const uint8_t* e0_ptr = reinterpret_cast<const uint8_t*>(e0);
  uint32_t e0_offset = static_cast<uint32_t>(e0_ptr - (base + header_size));
  const uint8_t* p0 = base + header_size + e0_offset + sizeof(record::AudioBlockEntry);
  for (size_t i = 0; i < audio_payload.size(); ++i)
    EXPECT_EQ(p0[i], audio_payload[i]);

  // 두 번째 엔트리 payload
  const record::BlockEntry* e1 = block.entries[1];
  const uint8_t* e1_ptr = reinterpret_cast<const uint8_t*>(e1);
  uint32_t e1_offset = static_cast<uint32_t>(e1_ptr - (base + header_size));
  const uint8_t* p1 = base + header_size + e1_offset + sizeof(record::UserBlockEntry);
  for (size_t i = 0; i < user_payload.size(); ++i)
    EXPECT_EQ(p1[i], user_payload[i]);

  // 세 번째 엔트리는 페이로드 없음
  const record::BlockEntry* e2 = block.entries[2];
  EXPECT_EQ(e2->timestamp, 3000);
}

// 엔트리 포인터가 serialized 내부를 가리키는지 확인하는 간단한 검사
TEST(BlockBuilderTest, EntriesPointInsideSerialized)
{
  auto opts = std::make_shared<TestOptions>(true);
  record::BlockBuilder builder(opts);

  record::BlockEntry be{};
  std::memset(&be, 0, sizeof(be));
  be.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));
  be.timestamp = 777;
  auto b = make_entry_buffer_from(be, nullptr, 0);
  builder.add_entry(b);

  record::DataBlock block = builder.pop_block();
  const uint8_t* base = block.serialized.data();
  uint32_t header_size = static_cast<uint32_t>(sizeof(record::BlockHeader));

  for (const auto* e : block.entries) {
    const uint8_t* p = reinterpret_cast<const uint8_t*>(e);
    // 엔트리 포인터가 serialized 범위 안에 있어야 함
    EXPECT_GE(p, base + header_size);
    EXPECT_LT(p, base + block.serialized.size());
  }
}

// duration 초과로 블록이 자동 분리되는지 테스트
TEST(BlockBuilderTest, FinalizeBeforeAddOnDurationExceeded)
{
  // max_duration = 5ms 설정
  auto opts = std::make_shared<record::BasicBlockBuilderOptions>(
    nx::milliseconds(5),
    1024 * 1024, // 충분히 큰 크기
    1000         // 충분히 큰 카운트
  );
  record::BlockBuilder builder(opts);

  // 시나리오: timestamps = [1, 2, 3, 6, 7, 8]
  // 예상 결과: [1,2,3] 블록과 [6,7,8] 블록

  record::BlockEntry be1{};
  std::memset(&be1, 0, sizeof(be1));
  be1.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be1.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));
  be1.timestamp = 1;

  record::BlockEntry be2 = be1;
  be2.timestamp = 2;

  record::BlockEntry be3 = be1;
  be3.timestamp = 3;

  record::BlockEntry be6 = be1;
  be6.timestamp = 6;

  record::BlockEntry be7 = be1;
  be7.timestamp = 7;

  record::BlockEntry be8 = be1;
  be8.timestamp = 8;

  // 엔트리 추가
  builder.add_entry(make_entry_buffer_from(be1, nullptr, 0)); // [1]
  builder.add_entry(make_entry_buffer_from(be2, nullptr, 0)); // [1,2]
  builder.add_entry(make_entry_buffer_from(be3, nullptr, 0)); // [1,2,3]

  // ts=6 추가 시 would_be_duration = 6-1 = 5 >= 5 → 블록 완료 [1,2,3]
  builder.add_entry(make_entry_buffer_from(be6, nullptr, 0)); // 새 블록 [6]

  // 첫 번째 블록 확인
  EXPECT_EQ(builder.get_block_count(), 1u);
  record::DataBlock block1 = builder.pop_block();
  EXPECT_EQ(block1.entries.size(), 3u);
  EXPECT_EQ(block1.header->start_timestamp, 1);
  EXPECT_EQ(block1.header->end_timestamp, 3);

  // 계속 추가
  builder.add_entry(make_entry_buffer_from(be7, nullptr, 0)); // [6,7]
  builder.add_entry(make_entry_buffer_from(be8, nullptr, 0)); // [6,7,8]

  // flush로 두 번째 블록 완성
  builder.flush();
  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block2 = builder.pop_block();
  EXPECT_EQ(block2.entries.size(), 3u);
  EXPECT_EQ(block2.header->start_timestamp, 6);
  EXPECT_EQ(block2.header->end_timestamp, 8);
}

// timestamp 검증 - 과거 엔트리 거부 테스트
TEST(BlockBuilderTest, RejectPastEntries)
{
  auto opts = std::make_shared<record::BasicBlockBuilderOptions>(
    nx::seconds(10), // 충분히 긴 duration
    1024 * 1024,
    1000);
  opts->enable_timestamp_validation(true); // timestamp 검증 활성화

  record::BlockBuilder builder(opts);

  record::BlockEntry be{};
  std::memset(&be, 0, sizeof(be));
  be.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));

  // 첫 엔트리: ts=10
  be.timestamp = 10;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 정상 엔트리: ts=20
  be.timestamp = 20;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 과거 엔트리 시도: ts=5 (10보다 작음) → 거부되어야 함
  be.timestamp = 5;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 과거 엔트리 시도: ts=15 (10보다 크지만 20보다 작음) → 허용 (start_timestamp=10
  // 기준)
  be.timestamp = 15;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 정상 엔트리: ts=30
  be.timestamp = 30;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 블록 완성
  builder.flush();
  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block = builder.pop_block();

  // ts=5는 거부되었으므로 4개 엔트리만 있어야 함 (10, 20, 15, 30)
  EXPECT_EQ(block.entries.size(), 4u);

  // 정렬 후: [10, 15, 20, 30]
  EXPECT_EQ(block.entries[0]->timestamp, 10);
  EXPECT_EQ(block.entries[1]->timestamp, 15);
  EXPECT_EQ(block.entries[2]->timestamp, 20);
  EXPECT_EQ(block.entries[3]->timestamp, 30);
}

// timestamp 검증 비활성화 테스트
TEST(BlockBuilderTest, AcceptPastEntriesWhenValidationDisabled)
{
  auto opts = std::make_shared<record::BasicBlockBuilderOptions>(
    nx::seconds(10),
    1024 * 1024,
    1000);
  // timestamp 검증 비활성화
  opts->enable_timestamp_validation(false);

  record::BlockBuilder builder(opts);

  record::BlockEntry be{};
  std::memset(&be, 0, sizeof(be));
  be.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));

  // ts = [10, 20, 5, 30] 순서로 추가
  be.timestamp = 10;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 20;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 5; // 과거 엔트리이지만 허용되어야 함
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 30;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  builder.flush();
  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block = builder.pop_block();

  // 모든 엔트리가 추가되어야 함
  EXPECT_EQ(block.entries.size(), 4u);

  // 정렬 후: [5, 10, 20, 30]
  EXPECT_EQ(block.entries[0]->timestamp, 5);
  EXPECT_EQ(block.entries[1]->timestamp, 10);
  EXPECT_EQ(block.entries[2]->timestamp, 20);
  EXPECT_EQ(block.entries[3]->timestamp, 30);
}

// 블록 경계에서 과거 엔트리 거부 테스트
TEST(BlockBuilderTest, RejectPastEntriesAcrossBlocks)
{
  auto opts = std::make_shared<record::BasicBlockBuilderOptions>(
    nx::milliseconds(5), // duration=5ms
    1024 * 1024,
    1000);
  opts->enable_timestamp_validation(true);

  record::BlockBuilder builder(opts);

  record::BlockEntry be{};
  std::memset(&be, 0, sizeof(be));
  be.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));

  // 첫 번째 블록: [10, 11, 12]
  be.timestamp = 10;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 11;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 12;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // ts=20 추가 시 duration 초과 → 첫 번째 블록 완료
  be.timestamp = 20;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 첫 번째 블록 확인
  EXPECT_EQ(builder.get_block_count(), 1u);

  // 두 번째 블록 시작 timestamp=20
  // 과거 엔트리 시도: ts=15 (첫 번째 블록과 두 번째 블록 사이) → 거부
  be.timestamp = 15;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  // 정상 엔트리: ts=21
  be.timestamp = 21;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  builder.flush();

  // 첫 번째 블록: [10, 11, 12]
  record::DataBlock block1 = builder.pop_block();
  EXPECT_EQ(block1.entries.size(), 3u);
  EXPECT_EQ(block1.header->start_timestamp, 10);

  // 두 번째 블록: [20, 21] (15는 거부됨)
  record::DataBlock block2 = builder.pop_block();
  EXPECT_EQ(block2.entries.size(), 2u);
  EXPECT_EQ(block2.header->start_timestamp, 20);
  EXPECT_EQ(block2.entries[0]->timestamp, 20);
  EXPECT_EQ(block2.entries[1]->timestamp, 21);
}

// 정렬 필요성 테스트: 1 3 2 4 5 시나리오
TEST(BlockBuilderTest, SortingNeededForOutOfOrderEntries)
{
  auto opts = std::make_shared<record::BasicBlockBuilderOptions>(
    nx::seconds(10), // 충분히 긴 duration
    1024 * 1024,
    1000);
  opts->enable_timestamp_validation(true);

  record::BlockBuilder builder(opts);

  record::BlockEntry be{};
  std::memset(&be, 0, sizeof(be));
  be.type = static_cast<uint8_t>(record::BlockType::kUserdata);
  be.header_size = static_cast<uint16_t>(sizeof(record::BlockEntry));

  // 시나리오: 1 3 2 4 5 순서로 입력
  be.timestamp = 1;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 3;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 2; // 역전되었지만 start_timestamp(1)보다 크므로 허용
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 4;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  be.timestamp = 5;
  builder.add_entry(make_entry_buffer_from(be, nullptr, 0));

  builder.flush();
  EXPECT_EQ(builder.get_block_count(), 1u);

  record::DataBlock block = builder.pop_block();

  // 모든 엔트리 추가됨
  EXPECT_EQ(block.entries.size(), 5u);

  // 정렬 후: [1, 2, 3, 4, 5]
  EXPECT_EQ(block.entries[0]->timestamp, 1);
  EXPECT_EQ(block.entries[1]->timestamp, 2);
  EXPECT_EQ(block.entries[2]->timestamp, 3);
  EXPECT_EQ(block.entries[3]->timestamp, 4);
  EXPECT_EQ(block.entries[4]->timestamp, 5);
}
