// 파일: rtsp_client.h
// 생성일: 2026-02-23
// 설명: 메인 RTSP 클라이언트

#pragma once

#include "nxnet/rtsp/rtsp_types.h"
#include "nxnet/rtsp/rtsp_error.h"
#include "nxnet/rtsp/rtsp_message.h"
#include "nxnet/rtsp/rtsp_connection.h"
#include "nxnet/rtsp/rtsp_session.h"
#include "nxnet/sdp/sdp_session.h"
#include "nxnet/rtp/rtp_receiver.h"
#include "nxnet/rtp/rtp_types.h"
#include "nxnet/auth/auth_provider.h"
#include "nxnet/auth/auth_types.h"
#include "nxcore/util/type_util.h"
#include "nxcore/media/media_type.h"

#include <nxcore/util/asio_type.h>
#include <expected>
#include <functional>
#include <memory>
#include <atomic>
#include <span>
#include <string>

namespace nx::net {

class RtspClient
{
public:
  explicit RtspClient(
    AsioContext& ioc,
    uint32_t connect_timeout_ms = 5000,
    uint32_t response_timeout_ms = 10000);

  ~RtspClient();

  NX_NON_COPYABLE_AND_MOVABLE(RtspClient);

  // ========================================================================
  // 연결 및 스트림 제어
  // ========================================================================

  // RTSP 서버 연결
  nx::awaitable<std::error_code> connect(const std::string& rtsp_url);

  // 미디어 정보 조회 (SDP 수신)
  nx::awaitable_expected<sdp::SdpSession> describe();

  // 전송 모드 설정 (각 미디어 트랙에 대해 SETUP)
  nx::awaitable<std::error_code>
  setup(RtspTransport preferred_transport = RtspTransport::kRtpTcp);

  // 스트림 재생 시작
  nx::awaitable<std::error_code> play(double start = 0.0, double end = -1.0);

  // 스트림 일시정지
  nx::awaitable<std::error_code> pause();

  // 스트림 종료
  nx::awaitable<std::error_code> teardown();

  // 연결 종료
  nx::awaitable<std::error_code> close();

  // ========================================================================
  // 미디어 데이터 수신
  // ========================================================================

  using MediaFrameCallback = std::function<void(
    uint32_t track_id,
    media::MediaType media_type,
    std::shared_ptr<std::vector<uint8_t>> frame_data,
    uint64_t timestamp_us,
    bool keyframe)>;

  void set_media_callback(MediaFrameCallback callback);

  /// 연결 끊김 콜백 (수신 루프가 비정상 종료 시 호출)
  using DisconnectCallback = std::function<void()>;
  void set_disconnect_callback(DisconnectCallback callback);

  /// 코덱 불일치 감지 콜백 (디패킷타이저에서 연속 오류 임계치 초과 시 호출)
  using CodecMismatchCallback
    = std::function<void(uint32_t track_id, uint32_t error_count)>;
  void set_codec_mismatch_callback(CodecMismatchCallback callback);

  // ========================================================================
  // 인증 설정
  // ========================================================================

  void set_auth_provider(std::unique_ptr<auth::AuthProvider> provider);

  // Basic/Digest 자동 협상
  void set_credentials(const std::string& username, const std::string& password);

  // ========================================================================
  // 상태 조회
  // ========================================================================

  bool is_connected() const noexcept;
  RtspSessionState state() const noexcept;

  // 트랙 수
  size_t track_count() const noexcept;

  // 트랙별 RTP 수신 통계 조회
  std::optional<rtp::RtpStatistics>
  get_track_statistics(uint32_t track_id) const noexcept;

  // 트랙별 미디어 타입 조회
  std::optional<media::MediaType> get_track_media_type(uint32_t track_id) const noexcept;

  // ========================================================================
  // 수신 루프 대기 시간 통계 (io_context starvation 감지용)
  // ========================================================================

  /// receive_interleaved_loop에서 co_await 완료까지 걸린 시간 통계
  struct ReceiveLoopStats
  {
    double avg_recv_wait_us = 0.0; // 평균 대기 시간 (µs)
    double max_recv_wait_us = 0.0; // 최대 대기 시간 (µs)
    uint64_t recv_count = 0;       // 수신 횟수 (스냅샷 구간)
  };

  /// 통계 조회 (호출 시 내부 카운터 리셋하여 구간 통계 반환)
  ReceiveLoopStats get_receive_loop_stats();

private:
  // RTSP 요청 전송 및 응답 수신
  nx::awaitable_expected<RtspResponse> send_request(const RtspRequest& request);

  // 인증이 필요한 경우 재시도 처리
  nx::awaitable_expected<RtspResponse> send_request_with_auth(RtspRequest request);

  // 요청에 인증 헤더 추가
  void apply_auth(RtspRequest& request);

  // Session 헤더에서 ID 및 타임아웃 추출
  void parse_session_header(const std::string& session_header);

  // 미디어별 Control URL 생성
  std::string build_control_url(const std::string& control) const;

  // TCP Interleaved 수신 루프
  nx::awaitable<void> receive_interleaved_loop();

  // Keep-alive 루프
  nx::awaitable<void> keep_alive_loop();

  AsioContext& m_ioc;
  std::unique_ptr<RtspConnection> m_connection;
  RtspSession m_session;

  // 미디어 트랙별 RTP 수신기
  struct TrackInfo
  {
    uint32_t track_id = 0;
    media::MediaType media_type = media::MediaType::kUnknown;
    uint8_t rtp_channel = 0;
    uint8_t rtcp_channel = 0;
    std::unique_ptr<rtp::RtpReceiver> receiver;
  };
  std::vector<TrackInfo> m_tracks;

  // 인증
  std::unique_ptr<auth::AuthProvider> m_auth_provider;
  auth::Credentials m_credentials;
  bool m_use_auto_auth = false;

  // 콜백
  MediaFrameCallback m_media_callback;
  DisconnectCallback m_disconnect_callback;
  CodecMismatchCallback m_codec_mismatch_callback;

  // URL 정보
  std::string m_rtsp_url;
  std::string m_host;
  uint16_t m_port = 554;

  // 타임아웃
  nx::milliseconds m_connect_timeout;
  nx::milliseconds m_response_timeout;

  // 수신 루프 제어
  std::atomic<bool> m_receiving{false};
  std::atomic<bool> m_receive_loop_active{false};

  // 수신 루프 대기 시간 통계 (원자 카운터)
  std::atomic<uint64_t> m_recv_total_wait_us{0};
  std::atomic<uint64_t> m_recv_max_wait_us{0};
  std::atomic<uint64_t> m_recv_count{0};
};

} // namespace nx::net
