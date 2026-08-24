// 파일: tcp_write_serializer.h
// 생성일: 2026-03-04
// 설명: TCP 소켓 쓰기 직렬화기 - 소켓 당 하나의 비동기 write 큐

#pragma once

#include "nxnet/rtp/rtp_frame_buffer.h"

#include <nxcore/util/type_util.h>
#include <nxcore/util/asio_type.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nx::net {

/// 쓰기 큐 항목
/// - inline 경로: 기존 vector<uint8_t> 데이터를 직접 보유
/// - scatter-gather 경로: SharedRtpFrame + TCP 인터리브 헤더로 zero-copy 전송
struct WriteEntry
{
  // 인라인 데이터 경로 (기존 submit(vector<uint8_t>) 호환)
  std::vector<uint8_t> inline_data;

  // scatter-gather 경로 (RtpFrameBuffer 공유, zero-copy)
  nx::rtp::SharedRtpFrame frame_ref;
  std::vector<std::array<uint8_t, 4>> headers; // 패킷당 TCP 인터리브 헤더 ($+ch+len)

  /// async_write에 전달할 const_buffer 시퀀스 생성 (데이터 복사 없음)
  std::vector<boost::asio::const_buffer> to_buffers() const;
};

/// TCP 소켓 쓰기 직렬화기
/// - 소켓 당 하나의 인스턴스로, 모든 write를 큐잉하고 순차적으로 async_write
/// - submit()은 스레드 안전하며 즉시 리턴 (non-blocking)
/// - 큐 초과 시 slow consumer로 판단하여 소켓 종료
///
/// 설계 의도:
/// RTSP TCP Interleaved 전송에서 video/audio transport가 동일 소켓을 공유하므로,
/// 동시 async_write 방지를 위해 소켓 레벨에서 쓰기를 직렬화합니다.
/// send_rtp()/send_rtcp() 호출은 UDP sendto()처럼 즉시 리턴합니다.
class TcpWriteSerializer : public std::enable_shared_from_this<TcpWriteSerializer>
{
  NX_NON_COPYABLE_AND_MOVABLE(TcpWriteSerializer);

public:
  /// 기본 최대 큐 크기 (항목 수)
  static constexpr size_t kDefaultMaxQueueSize = 500;

  /// @param socket TCP 소켓 (공유)
  /// @param max_queue_size 최대 큐 크기 (초과 시 slow consumer로 소켓 종료)
  explicit TcpWriteSerializer(
    std::shared_ptr<boost::asio::ip::tcp::socket> socket,
    size_t max_queue_size = kDefaultMaxQueueSize);

  ~TcpWriteSerializer();

  /// 바이너리 데이터 전송 요청 (스레드 안전, 즉시 리턴)
  /// RTP/RTCP 인터리브 프레임 전송에 사용
  /// @param data 전송할 데이터 (소유권 이전)
  void submit(std::vector<uint8_t> data);

  /// 배치 전송 요청 — 한 프레임의 모든 인터리브 데이터를 단일 strand post로 큐잉
  /// 패킷마다 개별 post 하는 대신 배치 전체를 한 번의 IOCP completion으로 처리
  /// @param batch 전송할 데이터 목록 (소유권 이전)
  void submit_batch(std::vector<std::vector<uint8_t>> batch);

  /// WriteEntry 직접 제출 — scatter-gather 경로 (zero-copy RTP 프레임 전송)
  /// @param entry 전송할 쓰기 항목 (소유권 이전)
  void submit_frame(WriteEntry entry);

  /// 문자열 데이터 전송 요청 (스레드 안전, 즉시 리턴)
  /// RTSP 응답 전송에 사용
  /// @param data 전송할 문자열 (소유권 이전)
  void submit(std::string data);

  /// 전송 가능 상태 확인
  bool is_active() const;

  /// 종료 (대기 중인 큐 폐기)
  /// 소켓은 외부(RtspServerSession)에서 관리하므로 여기서 닫지 않음
  void close();

  /// 통계: 현재 큐에 대기 중인 항목 수
  size_t queued_count() const;

  /// 통계: slow consumer로 인한 드롭 횟수
  size_t overflow_count() const;

private:
  /// 큐에서 다음 데이터를 async_write (strand 내에서만 호출)
  void do_write();

  /// slow consumer 감지 시 소켓 종료
  void handle_overflow();

  std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
  AsioStrand m_strand;

  // strand 내에서만 접근 (lock-free)
  std::deque<WriteEntry> m_queue;
  bool m_writing = false;

  size_t m_max_queue_size;

  // 원자적 통계/상태 (외부 스레드에서 조회 가능)
  std::atomic<size_t> m_queued_count{0};
  std::atomic<size_t> m_overflow_count{0};
  std::atomic<bool> m_active{true};
};

} // namespace nx::net
