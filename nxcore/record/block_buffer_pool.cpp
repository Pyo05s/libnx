// 파일: block_buffer_pool.cpp
// 생성일: 2026-03-30
// 설명: DataBlock 직렬화 버퍼를 재사용하는 메모리 풀 및 RAII 버퍼 래퍼 구현

#include "block_buffer_pool.h"

#include <algorithm>

namespace nx {
namespace record {

// PooledBuffer 구현

PooledBuffer::PooledBuffer(
  std::vector<uint8_t> buf, std::shared_ptr<BlockBufferPool> pool)
    : m_buffer(std::move(buf))
    , m_pool(std::move(pool))
{}

PooledBuffer::~PooledBuffer()
{
  return_to_pool();
}

PooledBuffer::PooledBuffer(PooledBuffer&& other) noexcept
    : m_buffer(std::move(other.m_buffer))
    , m_pool(std::move(other.m_pool))
{}

PooledBuffer&
PooledBuffer::operator=(PooledBuffer&& other) noexcept
{
  if (this != &other) {
    return_to_pool();
    m_buffer = std::move(other.m_buffer);
    m_pool = std::move(other.m_pool);
  }
  return *this;
}

void
PooledBuffer::return_to_pool()
{
  if (m_pool && m_buffer.capacity() > 0) {
    m_pool->release(std::move(m_buffer));
    m_pool.reset();
  }
}

// BlockBufferPool 구현

BlockBufferPool::BlockBufferPool(std::size_t max_free)
    : m_max_free_buffers(max_free)
{}

PooledBuffer
BlockBufferPool::acquire(std::size_t needed_size)
{
  std::vector<uint8_t> buf;

  {
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_free_buffers.empty()) {
      // capacity가 needed_size 이상인 버퍼를 우선 선택 (재할당 방지)
      auto it = std::find_if(
        m_free_buffers.begin(),
        m_free_buffers.end(),
        [needed_size](const std::vector<uint8_t>& b) {
          return b.capacity() >= needed_size;
        });

      if (it != m_free_buffers.end()) {
        buf = std::move(*it);
        m_free_buffers.erase(it);
      }
      else {
        // 적합한 버퍼가 없으면 마지막 버퍼 사용 (resize로 확장됨)
        buf = std::move(m_free_buffers.back());
        m_free_buffers.pop_back();
      }
    }
  }

  buf.resize(needed_size);
  return PooledBuffer(std::move(buf), shared_from_this());
}

void
BlockBufferPool::release(std::vector<uint8_t> buf)
{
  // 크기는 비우고 capacity는 유지
  buf.clear();

  std::lock_guard<std::mutex> lock(m_mutex);

  if (m_free_buffers.size() < m_max_free_buffers) {
    m_free_buffers.push_back(std::move(buf));
  }
  // 풀이 가득 찬 경우 buf는 여기서 해제됨
}

std::size_t
BlockBufferPool::free_count() const
{
  std::lock_guard<std::mutex> lock(m_mutex);
  return m_free_buffers.size();
}

} // namespace record
} // namespace nx
