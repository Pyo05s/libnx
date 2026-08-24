// 파일: rtsp_server.h
// 생성일: 2026-02-26
// 설명: RTSP 서버 - TCP 리스너 및 클라이언트 연결 관리

#pragma once

#include "nxnet/rtsp/rtsp_media_session.h"
#include "nxnet/rtsp/rtsp_server_session.h"

#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <cstdint>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx::net {

/// RTSP 서버
/// - 지정 포트에서 TCP 연결 수신
/// - 클라이언트별 RtspServerSession 생성 및 관리
/// - 미디어 세션 레지스트리를 통해 미디어 데이터 제공
/// - 프로토콜만 담당, 코덱/프레임은 관여하지 않음
/// - 반드시 make_shared<RtspServer>()로 생성해야 합니다
class RtspServer : public std::enable_shared_from_this<RtspServer>
{
  NX_NON_COPYABLE_AND_MOVABLE(RtspServer);

public:
  /// @param ioc io_context 참조
  /// @param registry 미디어 세션 레지스트리
  /// @param port 리슨 포트 (기본 8554)
  explicit RtspServer(
    AsioContext& ioc,
    std::shared_ptr<IRtspSessionRegistry> registry,
    uint16_t port = 8554);

  ~RtspServer();

  /// 서버 시작 (비동기 accept 루프 개시)
  /// @return 에러 코드 (성공 시 빈 에러)
  [[nodiscard]]
  nx::awaitable<std::error_code> start();

  /// 서버 중지 (모든 세션 종료)
  [[nodiscard]]
  nx::awaitable<void> stop();

  /// 실행 상태
  bool is_running() const;

  /// 리슨 포트
  uint16_t port() const;

  /// 활성 클라이언트 수
  size_t client_count() const;

private:
  /// TCP Accept 루프
  [[nodiscard]]
  nx::awaitable<void> accept_loop();

  /// 종료된 세션 정리
  void cleanup_sessions();

  AsioContext& m_ioc;
  std::shared_ptr<IRtspSessionRegistry> m_registry;
  uint16_t m_port;

  boost::asio::ip::tcp::acceptor m_acceptor;
  bool m_running = false;

  // 활성 세션 관리
  mutable std::mutex m_mutex;
  std::vector<std::shared_ptr<RtspServerSession>> m_sessions;
};

} // namespace nx::net
