// 파일: tcp_write_serializer.cpp
// 생성일: 2026-03-04
// 설명: TCP 소켓 쓰기 직렬화기 구현

#include "tcp_write_serializer.h"

#include <spdlog/spdlog.h>
#include <boost/asio/write.hpp>

namespace nx::net {

// ============================================================================
// WriteEntry
// ============================================================================

std::vector<boost::asio::const_buffer>
WriteEntry::to_buffers() const
{
  if (frame_ref) {
    auto count = frame_ref->packet_count();
    std::vector<boost::asio::const_buffer> bufs;
    bufs.reserve(count * 2);
    for (size_t i = 0; i < count; ++i) {
      bufs.emplace_back(headers[i].data(), headers[i].size());
      auto pkt = frame_ref->packet(i);
      bufs.emplace_back(pkt.data(), pkt.size());
    }
    return bufs;
  }
  return {boost::asio::const_buffer(inline_data.data(), inline_data.size())};
}

// ============================================================================
// TcpWriteSerializer
// ============================================================================

TcpWriteSerializer::TcpWriteSerializer(
  std::shared_ptr<boost::asio::ip::tcp::socket> socket, size_t max_queue_size)
    : m_socket(std::move(socket))
    , m_strand(boost::asio::make_strand(m_socket->get_executor()))
    , m_max_queue_size(max_queue_size)
{}

TcpWriteSerializer::~TcpWriteSerializer()
{
  // 소멸자에서는 shared_from_this() 사용 불가
  // 직접 상태 정리 (마지막 소유자이므로 스레드 경합 없음)
  m_active.store(false, std::memory_order_relaxed);
  m_queue.clear();
  m_writing = false;
  m_queued_count.store(0, std::memory_order_relaxed);
}

void
TcpWriteSerializer::submit(std::vector<uint8_t> data)
{
  if (!m_active.load(std::memory_order_relaxed) || data.empty()) {
    return;
  }

  WriteEntry entry;
  entry.inline_data = std::move(data);
  submit_frame(std::move(entry));
}

void
TcpWriteSerializer::submit(std::string data)
{
  std::vector<uint8_t> bytes(data.begin(), data.end());
  submit(std::move(bytes));
}

void
TcpWriteSerializer::submit_batch(std::vector<std::vector<uint8_t>> batch)
{
  if (!m_active.load(std::memory_order_relaxed) || batch.empty()) {
    return;
  }

  // 배치 전체를 단일 strand post로 처리 — IOCP completion 1회
  boost::asio::post(
    m_strand,
    [self = shared_from_this(), b = std::move(batch)]() mutable {
      if (!self->m_active.load(std::memory_order_relaxed)) {
        return;
      }

      // 큐 초과 여부: 배치 추가 후 한 번만 판정
      if (self->m_queue.size() + b.size() > self->m_max_queue_size) {
        self->handle_overflow();
        return;
      }

      for (auto& d : b) {
        WriteEntry entry;
        entry.inline_data = std::move(d);
        self->m_queue.push_back(std::move(entry));
      }
      self->m_queued_count.store(self->m_queue.size(), std::memory_order_relaxed);

      if (!self->m_writing) {
        self->do_write();
      }
    });
}

void
TcpWriteSerializer::submit_frame(WriteEntry entry)
{
  if (!m_active.load(std::memory_order_relaxed)) {
    return;
  }

  boost::asio::post(
    m_strand,
    [self = shared_from_this(), e = std::move(entry)]() mutable {
      if (!self->m_active.load(std::memory_order_relaxed)) {
        return;
      }

      if (self->m_queue.size() >= self->m_max_queue_size) {
        self->handle_overflow();
        return;
      }

      self->m_queue.push_back(std::move(e));
      self->m_queued_count.store(self->m_queue.size(), std::memory_order_relaxed);

      if (!self->m_writing) {
        self->do_write();
      }
    });
}

bool
TcpWriteSerializer::is_active() const
{
  return m_active.load(std::memory_order_relaxed) && m_socket && m_socket->is_open();
}

void
TcpWriteSerializer::close()
{
  if (!m_active.exchange(false, std::memory_order_relaxed)) {
    return;
  }

  // strand에서 큐 정리 (안전한 접근)
  boost::asio::post(m_strand, [self = shared_from_this()]() {
    self->m_queue.clear();
    self->m_writing = false;
    self->m_queued_count.store(0, std::memory_order_relaxed);
  });
}

size_t
TcpWriteSerializer::queued_count() const
{
  return m_queued_count.load(std::memory_order_relaxed);
}

size_t
TcpWriteSerializer::overflow_count() const
{
  return m_overflow_count.load(std::memory_order_relaxed);
}

void
TcpWriteSerializer::do_write()
{
  // strand 내에서만 호출 — lock 불필요
  if (m_queue.empty()) {
    m_writing = false;
    return;
  }

  m_writing = true;

  // front의 to_buffers()가 const_buffer 시퀀스를 생성
  // async_write가 BufferSequence를 값으로 캡처하므로 로컬 변수로 충분
  // const_buffer 내부 포인터는 m_queue.front()의 데이터를 가리키며,
  // pop_front()는 완료 핸들러에서만 호출하므로 수명 보장
  boost::asio::async_write(
    *m_socket,
    m_queue.front().to_buffers(),
    boost::asio::bind_executor(
      m_strand,
      [self = shared_from_this()](boost::system::error_code ec, size_t /*bytes*/) {
        if (ec) {
          if (ec != boost::asio::error::operation_aborted) {
            spdlog::debug("[TcpWriteSerializer] 전송 실패: {}", ec.message());
          }
          // 오류 시 큐 정리 및 중단
          self->m_queue.clear();
          self->m_writing = false;
          self->m_queued_count.store(0, std::memory_order_relaxed);
          return;
        }

        // close()에 의해 큐가 정리된 경우 안전하게 종료
        if (
          !self->m_active.load(std::memory_order_relaxed) || self->m_queue.empty()) {
          self->m_writing = false;
          self->m_queued_count.store(0, std::memory_order_relaxed);
          return;
        }

        // 성공: front 제거 후 다음 write 시도
        self->m_queue.pop_front();
        self->m_queued_count.store(self->m_queue.size(), std::memory_order_relaxed);
        self->do_write();
      }));
}

void
TcpWriteSerializer::handle_overflow()
{
  // strand 내에서 호출됨
  m_overflow_count.fetch_add(1, std::memory_order_relaxed);

  spdlog::warn(
    "[TcpWriteSerializer] 큐 초과 (size={}) → slow consumer, 소켓 종료",
    m_queue.size());

  // 소켓 종료 → 진행 중인 async_write에 operation_aborted 발생
  // 큐는 건드리지 않음: async_write 완료 핸들러(에러 경로)에서 안전하게 정리됨
  if (m_socket && m_socket->is_open()) {
    boost::system::error_code ec;
    m_socket->shutdown(boost::asio::ip::tcp::socket::shutdown_both, ec);
    m_socket->close(ec);
  }

  m_active.store(false, std::memory_order_relaxed);
}

} // namespace nx::net
