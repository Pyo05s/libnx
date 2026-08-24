// 파일: media_sink.h
// 생성일: 2026-02-26
// 설명: 미디어 싱크 인터페이스 - 파이프라인 데이터 소비 지점 (공용 라이브러리)

#pragma once

#include "media_frame.h"
#include "media_source.h"
#include <nxcore/util/type_util.h>

#include <nxcore/util/asio_type.h>
#include <cstddef>
#include <expected>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace nx {
namespace media {

/// 미디어 싱크 순수 가상 인터페이스
/// - RTSP 서버 출력, 녹화 출력 등의 기반 클래스
/// - 트랙 정보로 초기화 → 프레임 수신 → 종료
class IMediaSink
{
public:
  virtual ~IMediaSink() = default;

  /// 싱크 초기화 (트랙 정보 기반으로 출력 준비)
  /// @param tracks 소스에서 제공하는 트랙 정보
  /// @return 출력 URL 또는 에러
  [[nodiscard]]
  virtual nx::awaitable_expected<std::string>
  open(const std::vector<MediaTrackInfo>& tracks) = 0;

  /// 프레임 전송 (non-blocking 보장 필수)
  /// @param frame 전송할 미디어 프레임
  /// @note 이 함수는 호출 즉시 반환되어야 합니다.
  ///       무거운 처리(디스크 I/O, 인코딩, mutex 대기 등)는
  ///       내부적으로 별도 io_context/스레드에 비동기 위임해야 합니다.
  ///       호출자(MediaPipeline)의 스레드를 차단하지 마십시오.
  virtual void send_frame(const MediaFrame& frame) = 0;

  /// 싱크 종료 및 리소스 해제
  [[nodiscard]]
  virtual nx::awaitable<void> close() = 0;

  /// 싱크 이름 (로깅용)
  virtual std::string_view sink_name() const = 0;

  /// 출력 URL (open 후 유효)
  virtual std::string output_url() const = 0;

  /// 현재 연결된 클라이언트 수
  virtual std::size_t client_count() const = 0;
};

} // namespace media
} // namespace nx
