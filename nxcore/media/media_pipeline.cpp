// 파일: media_pipeline.cpp
// 생성일: 2026-02-26
// 설명: 미디어 파이프라인 구현

#include "media_pipeline.h"
#include "media_error.h"

#include <spdlog/spdlog.h>

namespace nx {
namespace media {

// ============================================================================
// 생성자/소멸자
// ============================================================================

MediaPipeline::MediaPipeline(
  AsioContext& ioc,
  std::unique_ptr<IMediaSource> source,
  std::unique_ptr<IMediaSink> sink,
  std::string session_id)
    : m_ioc(ioc)
    , m_strand(boost::asio::make_strand(m_ioc))
    , m_source(std::move(source))
    , m_sink(std::move(sink))
    , m_session_id(std::move(session_id))
{}

MediaPipeline::~MediaPipeline()
{
  if (m_running.load()) {
    spdlog::warn("[MediaPipeline:{}] 소멸자에서 비정상 종료 감지", m_session_id);
  }
}

// ============================================================================
// 파이프라인 시작
// ============================================================================

nx::awaitable_expected<std::string>
MediaPipeline::start()
{
  spdlog::info(
    "[MediaPipeline:{}] 파이프라인 시작: source={}",
    m_session_id,
    m_source->source_name());

  // 1. 소스 연결 및 미디어 정보 조회
  auto tracks_result = co_await m_source->open();
  if (!tracks_result) {
    spdlog::error(
      "[MediaPipeline:{}] 소스 연결 실패: {}",
      m_session_id,
      tracks_result.error().message());
    co_return std::unexpected(tracks_result.error());
  }

  m_tracks = std::move(*tracks_result);

  if (m_tracks.empty()) {
    spdlog::error("[MediaPipeline:{}] 소스에서 트랙 정보 없음", m_session_id);
    co_return std::unexpected(make_error_code(MediaErrc::kSourceNoTracks));
  }

  spdlog::info("[MediaPipeline:{}] 소스 트랙 {}개 발견", m_session_id, m_tracks.size());

  // 2. 싱크 초기화 (트랙 정보 전달)
  auto sink_result = co_await m_sink->open(m_tracks);
  if (!sink_result) {
    spdlog::error(
      "[MediaPipeline:{}] 싱크 초기화 실패: {}",
      m_session_id,
      sink_result.error().message());
    co_await m_source->close();
    co_return std::unexpected(sink_result.error());
  }

  auto output_url = *sink_result;

  spdlog::info("[MediaPipeline:{}] 싱크 초기화 완료: url={}", m_session_id, output_url);

  // 3. 소스에서 프레임 수신 시작 (콜백으로 싱크에 전달)
  m_running.store(true);

  auto ec = co_await m_source->start(
    [this](MediaFrame frame) { on_frame_received(std::move(frame)); });

  if (ec) {
    spdlog::error("[MediaPipeline:{}] 소스 시작 실패: {}", m_session_id, ec.message());
    m_running.store(false);
    co_await m_sink->close();
    co_await m_source->close();
    co_return std::unexpected(ec);
  }

  spdlog::info(
    "[MediaPipeline:{}] 파이프라인 가동 중: output={}",
    m_session_id,
    output_url);

  co_return output_url;
}

// ============================================================================
// 파이프라인 중지
// ============================================================================

nx::awaitable<void>
MediaPipeline::stop()
{
  if (!m_running.exchange(false)) {
    co_return;
  }

  spdlog::info(
    "[MediaPipeline:{}] 파이프라인 중지 (frames={}, bytes={})",
    m_session_id,
    m_frames_processed.load(),
    m_bytes_processed.load());

  // strand로 전환하여 pacing 타이머/큐 안전하게 정리
  co_await boost::asio::post(m_strand, boost::asio::use_awaitable);

  if (m_pacing_timer) {
    m_pacing_timer->cancel();
  }
  m_frame_queue = FrameQueue{};
  m_base_time_set = false;
  m_pacing_active = false;
  m_newest_timestamp = 0;

  // 소스 먼저 중지 (프레임 유입 차단)
  co_await m_source->close();

  // 싱크 종료
  co_await m_sink->close();

  spdlog::info("[MediaPipeline:{}] 파이프라인 중지 완료", m_session_id);
  co_return;
}

// ============================================================================
// 프레임 수신 핸들러 (소스 스레드에서 호출)
// - kLive: post 없이 호출 스레드에서 직접 dispatch (input thread 최소 점유)
// - kLiveBuffered/kPlayback: strand로 post하여 큐 접근 직렬화
// ============================================================================

void
MediaPipeline::on_frame_received(MediaFrame frame)
{
  if (!m_running.load()) {
    return;
  }

  if (m_mode == nx::media::PipelineMode::kLive) {
    // Live: 호출 스레드에서 직접 처리 (post 오버헤드 제거)
    // send_frame()은 non-blocking 계약 — input thread 즉시 반환
    if (frame.is_eof) {
      spdlog::info("[MediaPipeline:{}] 소스 EOF 수신 (Live)", m_session_id);
      m_running.store(false);
      if (m_completion_callback) {
        m_completion_callback(m_session_id);
      }
      return;
    }
    dispatch_frame(frame);
    return;
  }

  // kLiveBuffered / kPlayback: strand로 post하여 큐 접근 직렬화
  const auto post_time = std::chrono::steady_clock::now();

  boost::asio::post(m_strand, [this, f = std::move(frame), post_time]() mutable {
    // post → dispatch 지연 측정
    const auto dispatch_us = static_cast<uint64_t>(
      nx::duration_count<nx::microseconds>(std::chrono::steady_clock::now() - post_time));

    m_post_total_dispatch_us.fetch_add(dispatch_us, std::memory_order_relaxed);
    m_post_dispatch_count.fetch_add(1, std::memory_order_relaxed);
    // max 업데이트 (CAS 루프)
    {
      auto cur = m_post_max_dispatch_us.load(std::memory_order_relaxed);
      while (dispatch_us > cur
             && !m_post_max_dispatch_us.compare_exchange_weak(
               cur,
               dispatch_us,
               std::memory_order_relaxed)) {}
    }

    on_frame_received_via_strand(std::move(f));
  });
}

// ============================================================================
// strand에서 프레임 처리 (kLiveBuffered/kPlayback 전용)
// ============================================================================

void
MediaPipeline::on_frame_received_via_strand(MediaFrame frame)
{
  if (!m_running.load()) {
    return;
  }

  // EOF 마커: 소스가 모든 데이터를 전송 완료
  if (frame.is_eof) {
    spdlog::info("[MediaPipeline:{}] 소스 EOF 수신 (Buffered/Playback)", m_session_id);

    if (m_pacing_timer) {
      m_pacing_timer->cancel();
    }
    m_pacing_active = false;
    m_running.store(false);

    // 큐 잔여 프레임을 post 체인으로 1개씩 비동기 배출
    drain_eof_queue();
    return;
  }

  switch (m_mode) {
    case PipelineMode::kLive:
      // strand 경로에서 kLive가 호출되는 경우 없음 (방어 코드)
      dispatch_frame(frame);
      break;

    case PipelineMode::kLiveBuffered:
      if (frame.timestamp > m_newest_timestamp) {
        m_newest_timestamp = frame.timestamp;
      }
      m_frame_queue.push(std::move(frame));
      flush_one_frame();
      break;

    case PipelineMode::kPlayback:
      m_frame_queue.push(std::move(frame));
      if (!m_pacing_active) {
        m_pacing_active = true;
        schedule_next_frame();
      }
      break;
  }
}

// ============================================================================
// EOF 큐 비우기: post 체인으로 1개씩 비동기 배출
// - m_running == false 상태에서 실행 (추가 입력 없음)
// - 큐가 비면 completion_callback 즉시 호출
// ============================================================================

void
MediaPipeline::drain_eof_queue()
{
  if (m_frame_queue.empty()) {
    // 모든 프레임 배출 완료 → 완료 콜백 즉시 호출
    if (m_completion_callback) {
      m_completion_callback(m_session_id);
    }
    return;
  }

  auto frame = m_frame_queue.top();
  m_frame_queue.pop();
  dispatch_frame(frame);

  // 다음 프레임은 별도 post로 처리 (io_context 양보)
  boost::asio::post(m_strand, [this]() { drain_eof_queue(); });
}

// ============================================================================
// 프레임 전달 (통계 업데이트 포함)
// ============================================================================

void
MediaPipeline::dispatch_frame(const MediaFrame& frame)
{
  m_frames_processed.fetch_add(1, std::memory_order_relaxed);
  m_bytes_processed.fetch_add(
    frame.data ? frame.data->size() : 0,
    std::memory_order_relaxed);
  m_last_frame_time = std::chrono::steady_clock::now();

  m_sink->send_frame(frame);
}

// ============================================================================
// LiveBuffered: 버퍼에서 성숙된 프레임 1개 배출 (post 체인)
// - strand에서 실행 보장
// - 1개 dispatch 후 조건 충족 시 다음 프레임을 post로 예약
// ============================================================================

void
MediaPipeline::flush_one_frame()
{
  if (m_frame_queue.size() <= 1) {
    return;
  }

  auto oldest_ts = m_frame_queue.top().timestamp;
  auto threshold_ms = m_buffer_duration.count();

  if (m_newest_timestamp - oldest_ts < threshold_ms) {
    return;
  }

  dispatch_frame(m_frame_queue.top());
  m_frame_queue.pop();

  // 다음 성숙 프레임이 있으면 post로 예약 (io_context 양보)
  if (m_frame_queue.size() > 1) {
    boost::asio::post(m_strand, [this]() {
      if (m_running.load()) {
        flush_one_frame();
      }
    });
  }
}

// ============================================================================
// Playback: pacing 타이머 기반 프레임 배출
// ============================================================================

void
MediaPipeline::schedule_next_frame()
{
  // io_context 스레드에서만 호출됨 — mutex 불필요
  if (!m_running.load()) {
    m_pacing_active = false;
    return;
  }

  if (m_frame_queue.empty()) {
    m_pacing_active = false;
    return;
  }

  auto frame = m_frame_queue.top();

  if (!m_base_time_set) {
    // 첫 프레임: 즉시 전달, 기준 시각 설정
    m_frame_queue.pop();
    m_base_timestamp = frame.timestamp;
    m_base_wall_time = std::chrono::steady_clock::now();
    m_base_time_set = true;

    dispatch_frame(frame);
    // 재귀 대신 post로 변경 — 스택 안전 + io_context 양보
    boost::asio::post(m_strand, [this]() { schedule_next_frame(); });
    return;
  }

  // 프레임 타임스탬프와 기준 시각의 차이로 배출 시점 계산
  auto elapsed_ms = frame.timestamp - m_base_timestamp;
  if (elapsed_ms < 0) {
    elapsed_ms = 0;
  }

  auto target_wall
    = m_base_wall_time
      + nx::milliseconds(static_cast<int64_t>(elapsed_ms / m_playback_speed));

  auto now = std::chrono::steady_clock::now();
  auto delay = target_wall - now;

  if (delay <= nx::milliseconds::zero()) {
    m_frame_queue.pop();
    dispatch_frame(frame);
    // 재귀 대신 post로 변경 — 스택 오버플로우 방지 + io_context 양보
    boost::asio::post(m_strand, [this]() { schedule_next_frame(); });
    return;
  }

  // 타이머 설정 (단일 타이머 체인: 한 번에 하나만 활성)
  if (!m_pacing_timer) {
    m_pacing_timer = std::make_unique<AsioSteadyTimer>(m_strand);
  }

  m_pacing_timer->expires_after(std::chrono::duration_cast<nx::nanoseconds>(delay));

  m_pacing_timer->async_wait([this](const boost::system::error_code& ec) {
    if (ec || !m_running.load()) {
      m_pacing_active = false;
      return;
    }

    if (m_frame_queue.empty()) {
      m_pacing_active = false;
      return;
    }

    auto f = m_frame_queue.top();
    m_frame_queue.pop();
    dispatch_frame(f);
    schedule_next_frame();
  });
}

// ============================================================================
// 접근자
// ============================================================================

bool
MediaPipeline::is_running() const
{
  return m_running.load();
}

const std::string&
MediaPipeline::session_id() const
{
  return m_session_id;
}

std::string
MediaPipeline::output_url() const
{
  if (m_sink) {
    return m_sink->output_url();
  }
  return {};
}

const std::vector<MediaTrackInfo>&
MediaPipeline::tracks() const
{
  return m_tracks;
}

uint64_t
MediaPipeline::frames_processed() const
{
  return m_frames_processed.load(std::memory_order_relaxed);
}

uint64_t
MediaPipeline::bytes_processed() const
{
  return m_bytes_processed.load(std::memory_order_relaxed);
}

IMediaSink*
MediaPipeline::sink() const
{
  return m_sink.get();
}

IMediaSource*
MediaPipeline::source() const
{
  return m_source.get();
}

std::chrono::steady_clock::time_point
MediaPipeline::last_frame_time() const
{
  return m_last_frame_time;
}

MediaPipeline::PostDispatchStats
MediaPipeline::get_post_dispatch_stats()
{
  const auto count = m_post_dispatch_count.exchange(0, std::memory_order_relaxed);
  const auto total_us = m_post_total_dispatch_us.exchange(0, std::memory_order_relaxed);
  const auto max_us = m_post_max_dispatch_us.exchange(0, std::memory_order_relaxed);

  PostDispatchStats stats;
  stats.dispatch_count = count;
  stats.max_dispatch_us = static_cast<double>(max_us);
  stats.avg_dispatch_us
    = (count > 0) ? static_cast<double>(total_us) / static_cast<double>(count) : 0.0;
  return stats;
}

void
MediaPipeline::set_completion_callback(CompletionCallback cb)
{
  m_completion_callback = std::move(cb);
}

// ============================================================================
// 모드 설정
// ============================================================================

void
MediaPipeline::set_mode(PipelineMode mode)
{
  m_mode = mode;
  spdlog::info("[MediaPipeline:{}] 모드 설정: {}", m_session_id, static_cast<int>(mode));
}

PipelineMode
MediaPipeline::mode() const
{
  return m_mode;
}

void
MediaPipeline::set_buffer_duration(nx::milliseconds duration)
{
  m_buffer_duration = duration;
  spdlog::info("[MediaPipeline:{}] 버퍼 크기 설정: {}ms", m_session_id, duration.count());
}

void
MediaPipeline::set_playback_speed(double speed)
{
  m_playback_speed = speed;
  spdlog::info("[MediaPipeline:{}] 재생 속도 설정: {}x", m_session_id, speed);
}

} // namespace media
} // namespace nx
