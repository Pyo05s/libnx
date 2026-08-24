// 파일: media_frame_buffer.h
// 생성일: 2026-06-13
// 설명: MediaFrame 데이터 버퍼 풀 — 수신 경로에서 프레임당 힙 할당/해제 제거

#pragma once

#include <cstddef>
#include <memory>
#include <mutex>
#include <vector>

namespace nx::media {

/// 미디어 프레임 데이터의 shared_ptr 타입 별칭
using SharedFrameData = std::shared_ptr<std::vector<uint8_t>>;

/// vector<uint8_t> 버퍼 풀 — 수신 루프에서 매 프레임마다 힙 할당/해제가
/// 발생하는 병목을 제거하기 위해 사용한다.
///
/// 구현 전략: shared_ptr<vector> 슬롯을 use_count==1인 상태로 풀에 보관.
/// acquire() 시 슬롯의 shared_ptr을 복사하여 반환 (refcount 2).
/// 호출자가 소멸하면 refcount가 1로 돌아와 자동으로 풀 반환 완료.
/// → control block 할당이 슬롯 생성 시 1회뿐 (make_shared로 vector와 동시 할당).
///
/// 사용 방법:
///   auto pool = std::make_shared<MediaFrameBufferPool>();
///   auto buf  = pool->acquire();     // 풀에서 버퍼 획득
///   buf->resize(...);                // 데이터 채우기
///   frame.data = std::move(buf);     // 소유권 이전 (zero-copy)
///   // 마지막 shared_ptr이 소멸되면 자동으로 풀에 반환 (refcount 1로 복귀)
class MediaFrameBufferPool
{
public:
  static constexpr std::size_t kDefaultMaxPoolSize = 32;

  explicit MediaFrameBufferPool(std::size_t max_pool_size = kDefaultMaxPoolSize)
      : m_max_pool_size(max_pool_size)
  {
    m_slots.reserve(max_pool_size);
  }

  ~MediaFrameBufferPool() = default;

  MediaFrameBufferPool(const MediaFrameBufferPool&) = delete;
  MediaFrameBufferPool& operator=(const MediaFrameBufferPool&) = delete;

  /// 풀에서 버퍼를 획득한다.
  /// use_count==1인 유휴 슬롯을 복사하여 반환 — control block 재할당 없음.
  /// 모든 슬롯이 사용 중이면 새 슬롯을 make_shared로 생성한다.
  SharedFrameData acquire()
  {
    std::lock_guard lock(m_mutex);
    for (auto& slot : m_slots) {
      if (slot.use_count() == 1) {
        // 유휴 슬롯 — 데이터만 초기화 후 복사 반환
        slot->clear();
        return slot; // 복사: refcount 1→2
      }
    }

    // 유휴 슬롯 없음 — 새 슬롯 생성 (control block + vector 1회 alloc)
    if (m_slots.size() < m_max_pool_size) {
      m_slots.push_back(std::make_shared<std::vector<uint8_t>>());
      return m_slots.back(); // 복사: refcount 1→2
    }

    // 풀 포화 — 풀 외부 일반 할당 (드문 케이스)
    return std::make_shared<std::vector<uint8_t>>();
  }

private:
  std::size_t m_max_pool_size;
  std::mutex m_mutex;
  std::vector<SharedFrameData> m_slots; // 슬롯: use_count==1이면 유휴
};

} // namespace nx::media
