// 파일: block_buffer_pool.h
// 생성일: 2026-03-30
// 설명: DataBlock 직렬화 버퍼를 재사용하는 메모리 풀 및 RAII 버퍼 래퍼

#pragma once

#include <vector>
#include <memory>
#include <mutex>
#include <cstdint>

#include "../util/type_util.h"

namespace nx {
namespace record {

class BlockBufferPool;

// 풀 기반 RAII 버퍼 래퍼
// 풀이 연결된 경우 소멸 시 내부 버퍼를 풀에 반환하여 힙 할당을 제거합니다.
// 풀 없이도 일반 버퍼로 동작합니다. (테스트 등에서 직접 생성 시)
class PooledBuffer
{
public:
  PooledBuffer() = default;
  ~PooledBuffer();

  // 이동 전용
  PooledBuffer(PooledBuffer&& other) noexcept;
  PooledBuffer& operator=(PooledBuffer&& other) noexcept;
  NX_NON_COPYABLE(PooledBuffer);

  // std::vector<uint8_t> 호환 인터페이스
  uint8_t* data() { return m_buffer.data(); }
  const uint8_t* data() const { return m_buffer.data(); }
  std::size_t size() const { return m_buffer.size(); }
  bool empty() const { return m_buffer.empty(); }
  void resize(std::size_t new_size) { m_buffer.resize(new_size); }
  std::size_t capacity() const { return m_buffer.capacity(); }

private:
  friend class BlockBufferPool;

  PooledBuffer(std::vector<uint8_t> buf, std::shared_ptr<BlockBufferPool> pool);

  // 내부 버퍼를 풀에 반환
  void return_to_pool();

  std::vector<uint8_t> m_buffer;
  std::shared_ptr<BlockBufferPool> m_pool;
};

// DataBlock 직렬화 버퍼를 재사용하는 스레드 안전 메모리 풀
// BlockBuilder 당 하나씩 생성하여 세션별 독립적인 버퍼 재활용을 수행합니다.
// 1000세션 × 1블록/초 시나리오에서 힙 할당/해제를 제거합니다.
class BlockBufferPool : public std::enable_shared_from_this<BlockBufferPool>
{
public:
  static constexpr std::size_t kDefaultMaxFreeBuffers = 2;

  explicit BlockBufferPool(std::size_t max_free = kDefaultMaxFreeBuffers);
  ~BlockBufferPool() = default;

  NX_NON_COPYABLE_AND_MOVABLE(BlockBufferPool);

  // 풀에서 버퍼 획득 (재사용 가능한 버퍼가 있으면 재사용, 없으면 새 할당)
  PooledBuffer acquire(std::size_t needed_size);

  // 사용 완료된 버퍼를 풀에 반환 (PooledBuffer 소멸자에서 호출)
  void release(std::vector<uint8_t> buf);

  // 현재 풀에 대기 중인 버퍼 수
  std::size_t free_count() const;

private:
  mutable std::mutex m_mutex;
  std::vector<std::vector<uint8_t>> m_free_buffers;
  std::size_t m_max_free_buffers;
};

} // namespace record
} // namespace nx
