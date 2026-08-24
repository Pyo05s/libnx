// 파일: rtsp_media_session.h
// 생성일: 2026-02-26
// 설명: RTSP 미디어 세션 인터페이스 - RTSP 서버와 미디어 파이프라인 간 계약

#pragma once

#include "nxnet/rtp/rtp_frame_buffer.h"

#include <cstddef>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nx::net {

/// RTP 전송 인터페이스 (RTSP 서버가 생성하여 미디어 세션에 제공)
/// - TCP Interleaved 또는 UDP 소켓을 추상화
/// - 미디어 세션은 이 인터페이스를 통해 RTP/RTCP 패킷 전송
class IRtpTransport
{
public:
  virtual ~IRtpTransport() = default;

  /// RTP 패킷 전송
  virtual void send_rtp(std::span<const uint8_t> packet) = 0;

  /// RTP 패킷 배치 전송 (한 프레임의 모든 RTP 패킷을 단일 IOCP post로 처리)
  /// 기본 구현은 개별 send_rtp를 순차 호출 (TCP 구현체에서 오버라이드)
  virtual void send_rtp_batch(std::span<const std::vector<uint8_t>> packets);

  /// RTP 프레임 버퍼 전송 (zero-copy 경로)
  /// - shared_ptr로 다중 transport 간 패킷 데이터 공유
  /// - 기본 구현은 개별 send_rtp를 순차 호출
  virtual void send_rtp_frame(const nx::rtp::SharedRtpFrame& frame_buffer);

  /// RTCP 패킷 전송
  virtual void send_rtcp(std::span<const uint8_t> packet) = 0;

  /// 전송 가능 상태 확인
  virtual bool is_active() const = 0;

  /// transport 종료 (소켓/채널 닫기)
  virtual void close() = 0;
};

/// RTSP 미디어 세션 인터페이스
/// - RTSP 서버가 클라이언트 요청 처리 시 호출
/// - 앱이 구현하여 RTSP 서버에 등록
/// - 프로토콜만 다루고 코덱/프레임은 관여하지 않음
class IRtspMediaSession
{
public:
  virtual ~IRtspMediaSession() = default;

  /// SDP 설명 텍스트 (DESCRIBE 응답용)
  virtual std::string sdp() const = 0;

  /// 트랙 수 (SDP 미디어 라인 수)
  virtual size_t track_count() const = 0;

  /// 세션 이름 (URL 경로 매칭용)
  virtual std::string_view session_name() const = 0;

  /// PLAY 요청 처리 - 트랙별 RTP 전송 대상 등록
  /// @param track_index 트랙 인덱스 (0부터)
  /// @param transport 서버가 생성한 RTP 전송 인터페이스
  /// @param client_id 클라이언트 식별자 (RTSP session ID)
  virtual void on_play(
    size_t track_index,
    std::shared_ptr<IRtpTransport> transport,
    const std::string& client_id) = 0;

  /// TEARDOWN 요청 처리 - 트랙별 RTP 전송 대상 제거
  /// @param track_index 트랙 인덱스
  /// @param transport 제거할 RTP 전송 인터페이스
  /// @param client_id 클라이언트 식별자 (RTSP session ID)
  virtual void on_teardown(
    size_t track_index,
    std::shared_ptr<IRtpTransport> transport,
    const std::string& client_id) = 0;

  /// 활성 클라이언트 수 (재생 중인 transport 수)
  virtual size_t client_count() const = 0;
};

/// RTSP 미디어 세션 레지스트리 인터페이스
/// - RTSP 서버가 경로(path)로 미디어 세션을 검색할 때 사용
class IRtspSessionRegistry
{
public:
  virtual ~IRtspSessionRegistry() = default;

  /// URL 경로로 미디어 세션 검색
  /// @param path RTSP URL 경로 (예: "/live/ch1")
  /// @return 미디어 세션 (없으면 nullptr)
  virtual std::shared_ptr<IRtspMediaSession> find_session(std::string_view path) const
    = 0;
};

} // namespace nx::net
