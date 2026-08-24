// 파일: rtp_frame_buffer.cpp
// 생성일: 2026-04-26
// 설명: RTP 프레임 버퍼 풀 구현

#include "rtp_frame_buffer.h"

namespace nx::rtp {

RtpFrameBufferPool::RtpFrameBufferPool(size_t max_pool_size)
    : m_max_pool_size(max_pool_size)
{}

SharedRtpFrame
RtpFrameBufferPool::acquire()
{
  RtpFrameBuffer* raw = nullptr;

  {
    std::lock_guard lock(m_mutex);
    if (!m_pool.empty()) {
      raw = m_pool.back().release();
      m_pool.pop_back();
    }
  }

  if (!raw) {
    raw = new RtpFrameBuffer();
  }

  raw->clear();

  // custom deleter: refcount=0 시 풀에 반환
  return SharedRtpFrame(raw, [weak_pool = weak_from_this()](RtpFrameBuffer* p) {
    if (auto pool = weak_pool.lock()) {
      pool->release(p);
    }
    else {
      delete p;
    }
  });
}

size_t
RtpFrameBufferPool::pool_size() const
{
  std::lock_guard lock(m_mutex);
  return m_pool.size();
}

void
RtpFrameBufferPool::release(RtpFrameBuffer* buf)
{
  if (!buf) {
    return;
  }

  buf->clear();

  std::lock_guard lock(m_mutex);
  if (m_pool.size() < m_max_pool_size) {
    m_pool.emplace_back(buf);
  }
  else {
    delete buf;
  }
}

} // namespace nx::rtp
