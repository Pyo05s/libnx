// 파일: block_builder_options_unittest.cpp
// 생성일: 2025-11-24
// 설명: BlockBuilder Options 구현(시간 기반 및 크기 기반) 유닛 테스트

#include "nxcore/record/block_builder.h"
#include "nxcore/record/block_builder_options.h"
#include <gtest/gtest.h>
#include <memory>
#include <vector>
#include <cstring>

using namespace nx;

namespace {
// 시간 기반 옵션: 블록 시작부터 경과 시간이 threshold_ms 이상이면 블록 완료
class TimeBasedOptions : public record::BasicBlockBuilderOptions
{
public:
  explicit TimeBasedOptions(int64_t threshold_ms)
      : BasicBlockBuilderOptions(nx::milliseconds(threshold_ms), 100 * 1024 * 1024, 10000)
      , m_threshold_ms(threshold_ms)
  {}

  bool is_finished(record::BlockBuilder::Context& ctx) const override
  {
    if (ctx.pending_count == 0)
      return false;
    int64_t duration = ctx.end_timestamp - ctx.start_timestamp;
    return duration >= m_threshold_ms;
  }

private:
  int64_t m_threshold_ms;
};

// 크기 기반 옵션: pending_bytes가 threshold_bytes 이상이면 블록 완료
class SizeBasedOptions : public record::BasicBlockBuilderOptions
{
public:
  explicit SizeBasedOptions(std::size_t threshold_bytes)
      : BasicBlockBuilderOptions(nx::hours(1), threshold_bytes, 10000)
      , m_threshold_bytes(threshold_bytes)
  {}

  bool is_finished(record::BlockBuilder::Context& ctx) const override
  {
    return ctx.pending_bytes >= m_threshold_bytes && ctx.pending_count > 0;
  }

private:
  std::size_t m_threshold_bytes;
};

// 카운트 기반 옵션: pending_count가 threshold_count 이상이면 블록 완료
class CountBasedOptions : public record::BasicBlockBuilderOptions
{
public:
  explicit CountBasedOptions(std::size_t threshold_count)
      : BasicBlockBuilderOptions(nx::hours(1), 100 * 1024 * 1024, threshold_count)
      , m_threshold_count(threshold_count)
  {}

  bool is_finished(record::BlockBuilder::Context& ctx) const override
  {
    return ctx.pending_count >= m_threshold_count && ctx.pending_count > 0;
  }

private:
  std::size_t m_threshold_count;
};

// 키프레임 포함 기반 옵션: 대기중인 엔트리들 중 키프레임(I-프레임)이 포함되어 있으면
// 블록 완료
class KeyframeIncludedOptions : public record::BasicBlockBuilderOptions
{
public:
  KeyframeIncludedOptions()
      : BasicBlockBuilderOptions(nx::hours(1), 100 * 1024 * 1024, 10000)
  {}

  bool is_finished(record::BlockBuilder::Context& ctx) const override
  {
    // Context에 키프레임 플래그가 설정되어 있으면 블록을 완료
    return ctx.pending_count > 0 && ctx.contains_keyframe;
  }
};

// 테스트용 간단한 BlockEntryBuffer 팩토리 (오디오)
static std::shared_ptr<record::BlockEntryBuffer>
make_audio_entry(
  int64_t ts,
  uint32_t sample_rate,
  uint8_t channels,
  uint8_t bit_depth,
  std::size_t payload_size)
{
  auto buf = std::make_shared<record::BlockEntryBuffer>();
  auto entry = std::make_shared<record::AudioBlockEntry>();
  entry->type = static_cast<uint8_t>(record::BlockType::kAudio);
  entry->archive_type = 0;
  entry->timestamp = ts;
  entry->sample_rate = sample_rate;
  entry->bit_depth = bit_depth;
  entry->channels = channels;
  entry->payload_size = static_cast<uint32_t>(payload_size);
  // 저장한 payload_size는 실제 payload 길이를 의미하도록 설정

  entry->header_size = static_cast<uint16_t>(sizeof(record::AudioBlockEntry));
  buf->entry = entry;
  buf->payload = std::make_shared<std::vector<uint8_t>>(payload_size);
  return buf;
}

// 테스트용 비디오 엔트리 팩토리
static std::shared_ptr<record::BlockEntryBuffer>
make_video_entry(
  int64_t ts, uint8_t codec_type, uint8_t frame_type, std::size_t payload_size)
{
  auto buf = std::make_shared<record::BlockEntryBuffer>();
  auto entry = std::make_shared<record::VideoBlockEntry>();
  entry->type = static_cast<uint8_t>(record::BlockType::kVideo);
  entry->archive_type = 0;
  entry->timestamp = ts;
  entry->codec_type = codec_type;
  entry->frame_type = frame_type;
  entry->payload_size = static_cast<uint32_t>(payload_size);

  entry->header_size = static_cast<uint16_t>(sizeof(record::VideoBlockEntry));
  buf->entry = entry;
  buf->payload = std::make_shared<std::vector<uint8_t>>(payload_size);
  return buf;
}
} // namespace

TEST(OptionsTest, TimeBasedOptionsTriggers)
{
  auto opts = std::make_shared<TimeBasedOptions>(1000); // 1초(1000ms) 임계
  record::BlockBuilder builder(opts);

  // 2개의 짧은 오디오 엔트리를 추가, 총 duration < 1000ms
  auto e1 = make_audio_entry(
    10000,
    8000,
    1,
    16,
    1600); // 1600 bytes -> 100 samples -> 12.5ms
  builder.add_entry(e1);
  EXPECT_EQ(builder.get_block_count(), 0u);

  auto e2 = make_audio_entry(10050, 8000, 1, 16, 1600);
  builder.add_entry(e2);
  // duration ~ (10050+12) - 10000 = ~62ms < 1000
  EXPECT_EQ(builder.get_block_count(), 0u);

  // 큰 간격의 엔트리를 추가하여 트리거
  auto e3 = make_audio_entry(
    11050,
    8000,
    1,
    16,
    1600); // start=10000 end~11062 -> duration ~1062ms
  builder.add_entry(e3);
  EXPECT_EQ(builder.get_block_count(), 1u);
}

TEST(OptionsTest, SizeBasedOptionsTriggers)
{
  auto opts = std::make_shared<SizeBasedOptions>(3000); // 3KB 임계
  record::BlockBuilder builder(opts);

  auto e1 = make_audio_entry(20000, 8000, 1, 16, 1000);
  builder.add_entry(e1);
  EXPECT_EQ(builder.get_block_count(), 0u);

  auto e2 = make_audio_entry(20010, 8000, 1, 16, 1500);
  builder.add_entry(e2);
  EXPECT_EQ(builder.get_block_count(), 0u);

  auto e3 = make_audio_entry(20020, 8000, 1, 16, 800);
  builder.add_entry(e3);
  // pending_bytes ~ 1000+1500+800 + headers >= 3300 -> trigger
  EXPECT_EQ(builder.get_block_count(), 1u);
}

TEST(OptionsTest, CountBasedOptionsTriggers)
{
  auto opts = std::make_shared<CountBasedOptions>(3); // 3개 이상이면 블록 완료
  record::BlockBuilder builder(opts);

  auto e1 = make_audio_entry(30000, 8000, 1, 16, 500);
  builder.add_entry(e1);
  EXPECT_EQ(builder.get_block_count(), 0u);

  auto e2 = make_audio_entry(30010, 8000, 1, 16, 500);
  builder.add_entry(e2);
  EXPECT_EQ(builder.get_block_count(), 0u);

  auto e3 = make_audio_entry(30020, 8000, 1, 16, 500);
  builder.add_entry(e3);
  // pending_count == 3 -> trigger
  EXPECT_EQ(builder.get_block_count(), 1u);
}

TEST(OptionsTest, KeyframeIncludedOptionsTriggers)
{
  // 이 테스트는 키프레임(I-프레임)이 포함되면 블록을 종료하는 시나리오를
  // 검증합니다. 빌더는 키프레임 감지를 별도 로직으로 처리하지 않으므로, 여기서는
  // 키프레임이 추가되면 SizeBasedOptions와 결합하여 트리거되는 시나리오를
  // 사용합니다.

  // KeyframeIncludedOptions 를 사용해 키프레임(I-프레임) 포함 시 즉시 블록
  // 종료되는지 확인
  auto kf_opts = std::make_shared<KeyframeIncludedOptions>();
  record::BlockBuilder builder(kf_opts);

  // P-프레임 추가: 키프레임 없음
  auto v1 = make_video_entry(
    40000,
    static_cast<uint8_t>(record::VideoCodecType::kH264),
    static_cast<uint8_t>(record::VideoFrameType::kPFrame),
    1000);
  builder.add_entry(v1);
  EXPECT_EQ(builder.get_block_count(), 0u);

  // I-프레임 추가: Context.contains_keyframe 가 설정되어 블록이 완료되어야 함
  auto v2 = make_video_entry(
    40050,
    static_cast<uint8_t>(record::VideoCodecType::kH264),
    static_cast<uint8_t>(record::VideoFrameType::kIFrame),
    500);
  builder.add_entry(v2);
  EXPECT_EQ(builder.get_block_count(), 1u);
}
