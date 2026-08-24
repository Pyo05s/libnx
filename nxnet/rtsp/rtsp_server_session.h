// 파일: rtsp_server_session.h
// 생성일: 2026-02-26
// 설명: RTSP 서버 클라이언트 세션 - 개별 클라이언트 연결의 RTSP 프로토콜 처리

#pragma once

#include "nxnet/rtsp/rtsp_media_session.h"
#include "nxnet/rtsp/rtsp_types.h"
#include "nxnet/rtsp/tcp_write_serializer.h"

#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <cstdint>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace nx::net {

/// TCP Interleaved RTP 전송 구현
/// - TcpWriteSerializer를 통해 비동기 + 직렬화된 소켓 쓰기 수행
/// - send_rtp()/send_rtcp()는 즉시 리턴 (non-blocking)
class TcpInterleavedTransport
    : public IRtpTransport
    , public std::enable_shared_from_this<TcpInterleavedTransport>
{
public:
  /// @param serializer 소켓 쓰기 직렬화기 (소켓 당 1개, 공유)
  /// @param socket 기반 TCP 소켓 (서버측 능동 종료 시 직접 닫기 위해 공유)
  /// @param rtp_channel RTP 인터리브 채널 번호
  /// @param rtcp_channel RTCP 인터리브 채널 번호
  TcpInterleavedTransport(
    std::shared_ptr<TcpWriteSerializer> serializer,
    std::shared_ptr<boost::asio::ip::tcp::socket> socket,
    uint8_t rtp_channel,
    uint8_t rtcp_channel);

  void send_rtp(std::span<const uint8_t> packet) override;
  void send_rtp_batch(std::span<const std::vector<uint8_t>> packets) override;
  void send_rtp_frame(const nx::rtp::SharedRtpFrame& frame_buffer) override;
  void send_rtcp(std::span<const uint8_t> packet) override;
  bool is_active() const override;
  void close() override;

  std::size_t queued_count() const;
  std::size_t overflow_count() const;
  const TcpWriteSerializer* serializer_address() const;

private:
  /// '$' + channel(1) + length(2) + data 형식의 인터리브 프레임을 직렬화기에 제출
  void send_interleaved(uint8_t channel, std::span<const uint8_t> data);

  std::shared_ptr<TcpWriteSerializer> m_serializer;
  std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
  uint8_t m_rtp_channel;
  uint8_t m_rtcp_channel;
};

/// 트랙별 SETUP 상태
struct ServerTrackSetup
{
  size_t track_index = 0;
  std::string control_url;
  RtspTransportInfo transport_info;
  std::shared_ptr<IRtpTransport> rtp_transport;
  bool is_setup = false;
};

/// RTSP 서버 클라이언트 세션
/// - 개별 TCP 연결에 대한 RTSP 프로토콜 상태 머신
/// - OPTIONS/DESCRIBE/SETUP/PLAY/PAUSE/TEARDOWN 처리
/// - TCP Interleaved 전송 지원
class RtspServerSession : public std::enable_shared_from_this<RtspServerSession>
{
  NX_NON_COPYABLE_AND_MOVABLE(RtspServerSession);

public:
  /// @param socket 클라이언트 TCP 소켓 (소유권 이전)
  /// @param registry 미디어 세션 레지스트리
  explicit RtspServerSession(
    boost::asio::ip::tcp::socket socket, std::shared_ptr<IRtspSessionRegistry> registry);

  ~RtspServerSession();

  /// 세션 시작 (읽기 루프 개시)
  void start();

  /// 세션 종료
  void close();

  /// 클라이언트 주소 문자열
  std::string client_address() const;

private:
  /// RTSP 메시지 읽기 루프
  nx::awaitable<void> read_loop();

  /// RTSP 요청 파싱 (간단한 요청 파서)
  struct ParsedRequest
  {
    std::string method;
    std::string uri;
    std::string version;
    std::map<std::string, std::string> headers;
    std::string body;
    uint32_t cseq = 0;
  };

  static std::optional<ParsedRequest> parse_request(std::string_view data);

  /// 각 RTSP 메서드 핸들러
  nx::awaitable<void> handle_options(const ParsedRequest& req);
  nx::awaitable<void> handle_describe(const ParsedRequest& req);
  nx::awaitable<void> handle_setup(const ParsedRequest& req);
  nx::awaitable<void> handle_play(const ParsedRequest& req);
  nx::awaitable<void> handle_pause(const ParsedRequest& req);
  nx::awaitable<void> handle_teardown(const ParsedRequest& req);

  /// RTSP 응답 전송
  nx::awaitable<void> send_response(
    uint16_t status_code,
    const std::string& reason,
    uint32_t cseq,
    const std::map<std::string, std::string>& headers = {},
    const std::string& body = {});

  /// URL에서 경로 부분 추출
  static std::string extract_path(const std::string& uri);

  /// 세션 ID 생성
  static std::string generate_session_id();

  // 소켓 (shared_ptr로 serializer와 공유)
  std::shared_ptr<boost::asio::ip::tcp::socket> m_socket;
  std::shared_ptr<IRtspSessionRegistry> m_registry;

  // 소켓 쓰기 직렬화기 (RTSP 응답 + RTP/RTCP 인터리브 모두 이 경로로 전송)
  std::shared_ptr<TcpWriteSerializer> m_write_serializer;

  // RTSP 세션 상태
  RtspSessionState m_state = RtspSessionState::kConnected;
  std::string m_session_id;

  // 바인딩된 미디어 세션
  std::shared_ptr<IRtspMediaSession> m_media_session;

  // 트랙별 SETUP 상태
  std::vector<ServerTrackSetup> m_track_setups;

  // 수신 버퍼
  std::string m_recv_buffer;

  // 인터리브 채널 카운터
  uint8_t m_next_interleaved_channel = 0;

  bool m_running = false;
};

} // namespace nx::net
