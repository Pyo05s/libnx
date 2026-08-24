// 파일: media_pipeline.h
// 생성일: 2026-02-26
// 설명: 미디어 파이프라인 - source → sink 연결 및 프레임 전달 관리 (공용 라이브러리)

#pragma once

#include "media_source.h"
#include "media_sink.h"

#include <nxcore/util/type_util.h>
#include <nxcore/util/time_util.h>

#include <nxcore/util/asio_type.h>
#include <atomic>
#include <expected>
#include <functional>
#include <memory>
#include <queue>
#include <string>
#include <system_error>
#include <vector>

namespace nx {
namespace media {

/// 파이프라인 동작 모드
enum class PipelineMode : uint8_t
{
  kLive = 0,         ///< 패스스루: 소스 속도대로 즉시 전달 (default)
  kLiveBuffered = 1, ///< 소규모 정렬 버퍼: A/V 타임스탬프 순 정렬 후 전달
  kPlayback = 2      ///< 타이머 기반 pacing: 프레임 간 시간차대로 배출
};

/// 미디어 파이프라인
/// - IMediaSource → IMediaSink 연결
/// - 소스에서 프레임을 수신하여 싱크로 전달
/// - 비동기 실행 (코루틴 기반)
/// - PipelineMode에 따라 Live/LiveBuffered/Playback 동작
/// - 자체 통계 관리
class MediaPipeline
{
  NX_NON_COPYABLE_AND_MOVABLE(MediaPipeline);

public:
  /// @param ioc io_context 참조 (프레임 dispatch, 완료 콜백, pacing 타이머용)
  /// @param source 미디어 소스 (소유권 이전)
  /// @param sink 미디어 싱크 (소유권 이전)
  /// @param session_id 세션 식별자 (로깅용)
  MediaPipeline(
    AsioContext& ioc,
    std::unique_ptr<IMediaSource> source,
    std::unique_ptr<IMediaSink> sink,
    std::string session_id);

  ~MediaPipeline();

  /// 파이프라인 시작 (소스 연결 → 싱크 초기화 → 프레임 전달 시작)
  /// @return 출력 URL 또는 에러
  [[nodiscard]]
  nx::awaitable_expected<std::string> start();

  /// 파이프라인 중지
  [[nodiscard]]
  nx::awaitable<void> stop();

  /// 실행 상태 조회
  bool is_running() const;

  /// 세션 ID
  const std::string& session_id() const;

  /// 출력 URL
  std::string output_url() const;

  /// 트랙 정보 (start 후 유효)
  const std::vector<MediaTrackInfo>& tracks() const;

  /// 통계 조회
  uint64_t frames_processed() const;
  uint64_t bytes_processed() const;

  /// 마지막 프레임 수신 시각
  std::chrono::steady_clock::time_point last_frame_time() const;

  // ========================================================================
  // post → dispatch 지연 통계 (kLiveBuffered/kPlayback 전용, io_context starvation
  // 감지용) kLive 모드에서는 post하지 않으므로 통계가 수집되지 않습니다.
  // ========================================================================

  /// post(strand) → handler 실행까지 대기 시간 통계
  struct PostDispatchStats
  {
    double avg_dispatch_us = 0.0; // 평균 대기 시간 (µs)
    double max_dispatch_us = 0.0; // 최대 대기 시간 (µs)
    uint64_t dispatch_count = 0;  // 측정 횟수 (수집 구간)
  };

  /// 통계 조회 (호출 시 내부 카운터 리셋하여 구간 통계 반환)
  PostDispatchStats get_post_dispatch_stats();

  /// 파이프라인 모드 설정 (start() 호출 전에 설정)
  void set_mode(PipelineMode mode);

  /// 현재 모드 조회
  PipelineMode mode() const;

  /// LiveBuffered 모드의 버퍼 크기 설정 (기본: 200ms)
  void set_buffer_duration(nx::milliseconds duration);

  /// Playback 모드의 재생 속도 설정 (기본: 1.0x)
  void set_playback_speed(double speed);

  /// 파이프라인 완료 콜백 (EOF 또는 소스 종료 시 호출)
  /// @warning 이 콜백은 파이프라인의 strand 내부에서 동기적으로 호출됩니다.
  ///          구현체는 반드시 자체 io_context로 post하여 비동기 처리해야 합니다.
  ///          콜백 내에서 파이프라인을 직접 소멸시키면 정의되지 않은 동작이
  ///          발생합니다.
  using CompletionCallback = std::function<void(const std::string& session_id)>;

  /// 완료 콜백 설정
  void set_completion_callback(CompletionCallback cb);

  /// 싱크 접근 (레지스트리 등록 등 외부 연동용)
  IMediaSink* sink() const;

  /// 소스 접근 (통계 조회 등 외부 연동용)
  IMediaSource* source() const;

private:
  /// 프레임 수신 핸들러 (소스 스레드에서 호출)
  /// - kLive: 호출 스레드에서 직접 dispatch (post 없음)
  /// - kLiveBuffered/kPlayback: strand로 post하여 큐 안전성 보장
  void on_frame_received(MediaFrame frame);

  /// strand에서 프레임 처리 (kLiveBuffered/kPlayback 전용)
  void on_frame_received_via_strand(MediaFrame frame);

  /// 프레임을 싱크로 전달 (통계 업데이트 포함)
  void dispatch_frame(const MediaFrame& frame);

  /// EOF 수신 후 큐 잔여 프레임을 post 체인으로 1개씩 배출
  void drain_eof_queue();

  /// LiveBuffered: 버퍼에서 성숙된 프레임 1개 배출 (post 체인)
  void flush_one_frame();

  /// Playback: pacing 타이머 시작
  void schedule_next_frame();

  /// 타임스탬프 기준 프레임 비교기 (priority_queue용)
  struct FrameTimestampGreater
  {
    bool operator()(const MediaFrame& a, const MediaFrame& b) const
    {
      return a.timestamp > b.timestamp;
    }
  };

  using FrameQueue
    = std::priority_queue<MediaFrame, std::vector<MediaFrame>, FrameTimestampGreater>;

  AsioContext& m_ioc;
  AsioStrand m_strand;
  std::unique_ptr<IMediaSource> m_source;
  std::unique_ptr<IMediaSink> m_sink;
  std::string m_session_id;

  std::atomic<bool> m_running{false};
  std::vector<MediaTrackInfo> m_tracks;

  // 파이프라인 모드
  PipelineMode m_mode = PipelineMode::kLive;
  nx::milliseconds m_buffer_duration{200};
  double m_playback_speed = 1.0;

  // Pacing 타이머 (Playback/LiveBuffered 모드)
  std::unique_ptr<AsioSteadyTimer> m_pacing_timer;
  FrameQueue m_frame_queue;
  mstime_t m_base_timestamp = 0;
  mstime_t m_newest_timestamp = 0; // LiveBuffered: 큐에 추가된 최신 타임스탬프
  std::chrono::steady_clock::time_point m_base_wall_time;
  bool m_base_time_set = false;
  bool m_pacing_active = false; // Playback: 타이머 체인 활성화 여부

  // 통계
  std::atomic<uint64_t> m_frames_processed{0};
  std::atomic<uint64_t> m_bytes_processed{0};
  std::chrono::steady_clock::time_point m_last_frame_time;

  // post → dispatch 지연 통계 (원자 카운터)
  std::atomic<uint64_t> m_post_total_dispatch_us{0};
  std::atomic<uint64_t> m_post_max_dispatch_us{0};
  std::atomic<uint64_t> m_post_dispatch_count{0};

  // 완료 콜백
  CompletionCallback m_completion_callback;
};

} // namespace media
} // namespace nx
