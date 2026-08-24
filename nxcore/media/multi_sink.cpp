// 파일: multi_sink.cpp
// 생성일: 2026-03-10
// 설명: MultiSink 구현

#include "multi_sink.h"

#include "nxcore/util/scoped_perf_timer.h"
#include <algorithm>
#include <ranges>
#include <spdlog/spdlog.h>

namespace nx {
namespace media {

nx::awaitable_expected<std::string>
MultiSink::open(const std::vector<MediaTrackInfo>& tracks)
{
  // 트랙 정보 저장 (이후 add_sink 시 재사용)
  std::vector<IMediaSink*> sinks_to_open;
  {
    std::lock_guard lock(m_mutex);
    m_tracks = tracks;
    m_opened = true;
    for (auto& entry : m_sinks) {
      sinks_to_open.push_back(entry.sink.get());
    }
  }

  for (std::size_t i = 0; i < sinks_to_open.size(); ++i) {
    auto result = co_await sinks_to_open[i]->open(tracks);
    if (!result) {
      spdlog::error("[MultiSink] 싱크[{}] open 실패: {}", i, result.error().message());
      co_return result;
    }
    if (i == 0) {
      m_primary_url = *result;
    }
  }

  co_return m_primary_url;
}

void
MultiSink::send_frame(const MediaFrame& frame)
{
  // thread_local 벡터로 snapshot 할당 제거 (호출 스레드당 1회 할당 후 재사용)
  thread_local std::vector<IMediaSink*> snapshot;
  snapshot.clear();

  {
    std::lock_guard lock(m_mutex);
    snapshot.reserve(m_sinks.size());
    for (const auto& entry : m_sinks) {
      snapshot.push_back(entry.sink.get());
    }
  }

  for (auto* sink : snapshot) {
    sink->send_frame(frame);
  }
}

nx::awaitable<void>
MultiSink::close()
{
  std::vector<IMediaSink*> sinks_to_close;
  {
    std::lock_guard lock(m_mutex);
    m_opened = false;
    for (auto& entry : m_sinks) {
      sinks_to_close.push_back(entry.sink.get());
    }
  }

  for (auto* sink : sinks_to_close) {
    co_await sink->close();
  }
}

std::string_view
MultiSink::sink_name() const
{
  return "MultiSink";
}

std::string
MultiSink::output_url() const
{
  return m_primary_url;
}

std::size_t
MultiSink::client_count() const
{
  std::lock_guard lock(m_mutex);
  std::size_t total = 0;
  for (const auto& entry : m_sinks) {
    total += entry.sink->client_count();
  }
  return total;
}

nx::awaitable_expected<int64_t>
MultiSink::add_sink(std::shared_ptr<IMediaSink> sink)
{
  // open 상태 여부와 트랙 정보를 잠금 내에서 복사
  bool should_open = false;
  std::vector<MediaTrackInfo> tracks_copy;
  {
    std::lock_guard lock(m_mutex);
    should_open = m_opened;
    if (should_open) {
      tracks_copy = m_tracks;
    }
  }

  // 잠금 해제 상태에서 open 호출 (비동기 I/O 가능)
  if (should_open) {
    auto result = co_await sink->open(tracks_copy);
    if (!result) {
      spdlog::error("[MultiSink] 추가 싱크 open 실패: {}", result.error().message());
      co_return std::unexpected(result.error());
    }
  }

  std::lock_guard lock(m_mutex);
  auto id = m_next_id++;
  spdlog::debug("[MultiSink] 싱크 추가: id={}, total={}", id, m_sinks.size() + 1);
  m_sinks.push_back(SinkEntry{id, std::move(sink)});
  co_return id;
}

nx::awaitable<void>
MultiSink::remove_sink(int64_t id)
{
  std::shared_ptr<IMediaSink> to_close;
  {
    std::lock_guard lock(m_mutex);
    auto it =
      std::ranges::find_if(m_sinks, [id](const SinkEntry& e) { return e.id == id; });
    if (it != m_sinks.end()) {
      to_close = std::move(it->sink);
      m_sinks.erase(it);
      spdlog::debug("[MultiSink] 싱크 제거: id={}, remaining={}", id, m_sinks.size());
    }
  }

  if (to_close) {
    co_await to_close->close();
  }
}

} // namespace media
} // namespace nx
