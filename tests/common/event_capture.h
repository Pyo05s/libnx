// 파일: event_capture.h
// 생성일: 2026-03-09
// 설명: EventBus 이벤트 캡처 테스트 헬퍼

#pragma once

#include <common/event_bus.h>
#include <nxcore/util/time_util.h>

#include <nxcore/util/asio_type.h>

#include <algorithm>
#include <condition_variable>
#include <mutex>
#include <vector>

namespace test {

/// 특정 이벤트 타입의 발행을 캡처하는 테스트 헬퍼
/// - 스레드 안전한 이벤트 수집
/// - 동기 대기 (wait_for) 지원
/// - 이벤트 필터링/검색 유틸리티
///
/// 사용 예:
/// @code
///   EventCapture<DeviceStateChangedEvent> capture(event_bus, runner.io_context());
///   // ... 이벤트를 유발하는 작업 수행 ...
///   runner.poll_for(nx::milliseconds(100));
///   EXPECT_TRUE(capture.has_event([](const auto& e) {
///       return e.device_guid == "guid-001";
///   }));
/// @endcode
template <typename EventT>
class EventCapture
{
public:
  /// @param bus EventBus 참조
  /// @param ioc 구독 핸들러가 실행될 io_context
  explicit EventCapture(nx::EventBus& bus, AsioContext& ioc)
      : m_bus(bus)
  {
    m_subscription_id
      = bus.subscribe<EventT>(ioc.get_executor(), [this](const EventT& evt) {
          {
            std::lock_guard lock(m_mutex);
            m_events.push_back(evt);
          }
          m_cv.notify_all();
        });
  }

  ~EventCapture() { m_bus.unsubscribe(m_subscription_id); }

  // 복사/이동 금지
  EventCapture(const EventCapture&) = delete;
  EventCapture& operator=(const EventCapture&) = delete;
  EventCapture(EventCapture&&) = delete;
  EventCapture& operator=(EventCapture&&) = delete;

  /// 캡처된 이벤트 수
  std::size_t count() const
  {
    std::lock_guard lock(m_mutex);
    return m_events.size();
  }

  /// 캡처된 이벤트 전체 복사 반환
  std::vector<EventT> events() const
  {
    std::lock_guard lock(m_mutex);
    return m_events;
  }

  /// 이벤트가 비어있는지 확인
  bool empty() const
  {
    std::lock_guard lock(m_mutex);
    return m_events.empty();
  }

  /// 조건을 만족하는 이벤트 존재 여부
  template <typename Predicate>
  bool has_event(Predicate pred) const
  {
    std::lock_guard lock(m_mutex);
    return std::ranges::any_of(m_events, pred);
  }

  /// 조건을 만족하는 이벤트 개수
  template <typename Predicate>
  std::size_t count_if(Predicate pred) const
  {
    std::lock_guard lock(m_mutex);
    return static_cast<std::size_t>(std::ranges::count_if(m_events, pred));
  }

  /// 가장 최근 캡처된 이벤트 반환 (비어있으면 nullopt)
  std::optional<EventT> last() const
  {
    std::lock_guard lock(m_mutex);
    if (m_events.empty()) {
      return std::nullopt;
    }
    return m_events.back();
  }

  /// 최소 expected_count개의 이벤트가 수집될 때까지 대기
  /// @return 타임아웃 내 조건 충족 시 true
  bool wait_for(
    std::size_t expected_count, nx::milliseconds timeout = nx::milliseconds(3000)) const
  {
    std::unique_lock lock(m_mutex);
    return m_cv.wait_for(lock, timeout, [&] {
      return m_events.size() >= expected_count;
    });
  }

  /// 조건을 만족하는 이벤트가 수집될 때까지 대기
  /// @return 타임아웃 내 조건 충족 시 true
  template <typename Predicate>
  bool
  wait_for_event(Predicate pred, nx::milliseconds timeout = nx::milliseconds(3000)) const
  {
    std::unique_lock lock(m_mutex);
    return m_cv.wait_for(lock, timeout, [&] {
      return std::ranges::any_of(m_events, pred);
    });
  }

  /// 캡처된 이벤트 초기화
  void clear()
  {
    std::lock_guard lock(m_mutex);
    m_events.clear();
  }

private:
  nx::EventBus& m_bus;
  nx::SubscriptionId m_subscription_id;

  mutable std::mutex m_mutex;
  mutable std::condition_variable m_cv;
  std::vector<EventT> m_events;
};

} // namespace test
