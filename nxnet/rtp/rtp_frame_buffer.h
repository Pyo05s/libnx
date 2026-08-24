// 파일: rtp_frame_buffer.h
// 생성일: 2026-04-26
// 설명: RTP 프레임 버퍼 및 버퍼 풀 — 패킷화 결과의 zero-copy 공유 전송 지원

#pragma once

#include <nxcore/util/type_util.h>

#include <array>
#include <cstdint>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

namespace nx::rtp {

/// 한 프레임의 모든 RTP 패킷을 보관하는 연속 버퍼
/// - Packetizer가 이 버퍼에 직접 기록 (중간 vector 없음)
/// - shared_ptr로 다중 transport에 공유 (복사 0)
/// - 풀에서 획득하고, refcount=0 시 풀에 자동 반환
struct RtpFrameBuffer
{
  /// 모든 RTP 패킷이 연속 저장되는 데이터 영역
  std::vector<uint8_t> data;

  /// 각 패킷의 시작 오프셋 (마지막 원소 = data.size(), sentinel)
  /// 패킷 i: data[packet_offsets[i] .. packet_offsets[i+1])
  std::vector<uint32_t> packet_offsets;

  /// 패킷 수
  size_t packet_count() const
  {
    return packet_offsets.size() > 1 ? packet_offsets.size() - 1 : 0;
  }

  /// i번째 패킷 접근
  std::span<const uint8_t> packet(size_t i) const
  {
    return {data.data() + packet_offsets[i], packet_offsets[i + 1] - packet_offsets[i]};
  }

  /// 다음 프레임 기록 전 초기화 (capacity 유지 — 재할당 0)
  void clear()
  {
    data.clear();
    packet_offsets.clear();
  }

  /// 현재 패킷 기록 시작
  void begin_packet() { packet_offsets.push_back(static_cast<uint32_t>(data.size())); }

  /// 현재 패킷 기록 종료 (sentinel 오프셋 갱신)
  void end_packet()
  {
    // 마지막 sentinel은 finalize()에서 추가
  }

  /// 모든 패킷 기록 완료 후 호출 — sentinel 오프셋 추가
  void finalize() { packet_offsets.push_back(static_cast<uint32_t>(data.size())); }

  /// 단일 바이트 추가
  void append(uint8_t byte) { data.push_back(byte); }

  /// 바이트 시퀀스 추가
  void append(std::span<const uint8_t> bytes)
  {
    data.insert(data.end(), bytes.begin(), bytes.end());
  }

  /// 16비트 Big-Endian 추가
  void append_u16_be(uint16_t value)
  {
    std::array<uint8_t, 2> buf
      = {static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value & 0xFF)};
    data.insert(data.end(), buf.begin(), buf.end());
  }

  /// 32비트 Big-Endian 추가
  void append_u32_be(uint32_t value)
  {
    std::array<uint8_t, 4> buf = {
      static_cast<uint8_t>(value >> 24),
      static_cast<uint8_t>(value >> 16),
      static_cast<uint8_t>(value >> 8),
      static_cast<uint8_t>(value & 0xFF)};
    data.insert(data.end(), buf.begin(), buf.end());
  }

  /// 용량 사전 확보
  void reserve(size_t data_hint, size_t packet_count_hint)
  {
    data.reserve(data_hint);
    packet_offsets.reserve(packet_count_hint + 1);
  }
};

using SharedRtpFrame = std::shared_ptr<RtpFrameBuffer>;

class RtpFrameBufferPool;

/// RTP 프레임 버퍼 풀
/// - acquire()로 버퍼 획득, shared_ptr 소멸 시 자동 풀 반환
/// - warm-up 이후 힙 할당 0회 (capacity가 유지됨)
class RtpFrameBufferPool : public std::enable_shared_from_this<RtpFrameBufferPool>
{
  NX_NON_COPYABLE_AND_MOVABLE(RtpFrameBufferPool);

public:
  /// @param max_pool_size 풀에 보관할 최대 버퍼 수 (초과분은 해제)
  explicit RtpFrameBufferPool(size_t max_pool_size = 64);
  ~RtpFrameBufferPool() = default;

  /// 풀에서 RtpFrameBuffer 획득 (clear() 완료 상태)
  /// shared_ptr 소멸 시 custom deleter가 풀에 자동 반환
  SharedRtpFrame acquire();

  /// 현재 풀에 대기 중인 버퍼 수
  size_t pool_size() const;

private:
  void release(RtpFrameBuffer* buf);

  mutable std::mutex m_mutex;
  std::vector<std::unique_ptr<RtpFrameBuffer>> m_pool;
  size_t m_max_pool_size;
};

} // namespace nx::rtp
