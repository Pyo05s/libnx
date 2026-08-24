// 파일: rtsp_connection.h
// 생성일: 2026-02-23
// 설명: RTSP 연결 관리 (TCP)

#pragma once

#include "nxnet/rtsp/rtsp_message.h"
#include "nxnet/rtsp/rtsp_error.h"

#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <nxcore/util/asio_type.h>
#include <expected>
#include <memory>
#include <span>
#include <string>
#include <vector>

namespace nx::net {

class RtspConnection
{
public:
  explicit RtspConnection(
    AsioContext& ioc,
    nx::milliseconds connect_timeout = nx::seconds(5),
    nx::milliseconds response_timeout = nx::seconds(10));

  ~RtspConnection();

  NX_NON_COPYABLE_AND_MOVABLE(RtspConnection);

  // TCP 연결 수립
  nx::awaitable<std::error_code> connect(const std::string& host, uint16_t port);

  // RTSP 요청 전송
  nx::awaitable<std::error_code> send(const std::string& data);

  // RTSP 응답 수신
  nx::awaitable_expected<RtspResponse> receive_response();

  // TCP Interleaved 데이터 수신 ($ 프레임)
  // 반환: {channel, data} — data는 다음 receive_interleaved_frame() 호출 전까지
  // 유효한 span
  struct InterleavedFrame
  {
    uint8_t channel = 0;
    std::span<const uint8_t> data;
  };

  nx::awaitable_expected<InterleavedFrame> receive_interleaved_frame();

  // 연결 종료
  nx::awaitable<void> close();

  // 상태 조회
  bool is_connected() const noexcept;

  // 내부 소켓 접근 (RTP TCP Interleaved 모드용)
  boost::asio::ip::tcp::socket& socket() noexcept { return m_socket; }

private:
  // 버퍼에서 RTSP 응답 또는 인터리브드 프레임 분리
  nx::awaitable<std::error_code> read_more_data();

  // 타임아웃 적용 데이터 읽기
  nx::awaitable<std::error_code> read_with_timeout(AsioSteadyTimer& timer);

  // 소비된 선두 바이트를 제거하여 버퍼 선두를 m_recv_offset으로 이동
  void compact();

  // 미소비 가용 바이트 수
  size_t available() const noexcept { return m_recv_buffer.size() - m_recv_offset; }

  AsioContext& m_ioc;
  boost::asio::ip::tcp::socket m_socket;
  nx::milliseconds m_connect_timeout;
  nx::milliseconds m_response_timeout;

  // 수신 버퍼 — erase 없이 오프셋으로 소비, kCompactThreshold 초과 시 컴팩션
  std::vector<uint8_t> m_recv_buffer;
  size_t m_recv_offset = 0;     // 미소비 데이터 시작 위치
  size_t m_pending_consume = 0; // 다음 호출 시 확정 소비할 바이트 수
  bool m_connected = false;
};

} // namespace nx::net
