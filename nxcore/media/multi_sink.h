// 파일: multi_sink.h
// 생성일: 2026-03-10
// 설명: 다중 싱크 팬아웃 - 단일 소스에서 복수 싱크로 프레임 분배

#pragma once

#include "../util/type_util.h"
#include "media_sink.h"

#include <cstdint>
#include <expected>
#include <memory>
#include <mutex>
#include <nxcore/util/asio_type.h>
#include <string>
#include <vector>

namespace nx {
namespace media {

/// 다중 싱크 팬아웃
/// - 등록된 모든 싱크에 동일한 프레임을 전달
/// - 런타임에 싱크 추가/제거 가능 (파이프라인 실행 중에도 안전)
/// - output_url은 첫 번째 등록 싱크의 URL 반환 (주 싱크)
/// - client_count는 모든 싱크의 합산 반환
class MultiSink : public IMediaSink
{
  NX_NON_COPYABLE_AND_MOVABLE(MultiSink);

public:
  MultiSink() = default;
  ~MultiSink() override = default;

  // ========================================================================
  // IMediaSink
  // ========================================================================

  [[nodiscard]]
  nx::awaitable_expected<std::string>
  open(const std::vector<MediaTrackInfo>& tracks) override;

  /// 스냅샷 기반으로 등록된 모든 싱크에 프레임 전달 (thread-safe)
  void send_frame(const MediaFrame& frame) override;

  [[nodiscard]]
  nx::awaitable<void> close() override;

  std::string_view sink_name() const override;

  /// 첫 번째 등록 싱크의 URL 반환 (없으면 빈 문자열)
  std::string output_url() const override;

  /// 모든 싱크의 client_count 합산 반환
  std::size_t client_count() const override;

  // ========================================================================
  // 동적 싱크 관리
  // ========================================================================

  /// 싱크 추가
  /// - 이미 open 상태면 즉시 open(tracks) 호출
  /// - shared_ptr 공유 소유권: MultiSink와 외부가 함께 소유함
  /// @return 제거 시 사용할 고유 싱크 ID, 또는 에러
  [[nodiscard]]
  nx::awaitable_expected<int64_t> add_sink(std::shared_ptr<IMediaSink> sink);

  /// 싱크 제거 및 close 호출
  /// @param id add_sink 반환값
  [[nodiscard]]
  nx::awaitable<void> remove_sink(int64_t id);

private:
  struct SinkEntry
  {
    int64_t id;
    std::shared_ptr<IMediaSink> sink;
  };

  mutable std::mutex m_mutex;
  int64_t m_next_id{0};
  std::vector<SinkEntry> m_sinks;

  bool m_opened{false};
  std::vector<MediaTrackInfo> m_tracks; // open() 시 저장 → 후속 add_sink에 재사용
  std::string m_primary_url;            // 첫 번째 싱크의 URL
};

} // namespace media
} // namespace nx
