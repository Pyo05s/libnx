// 파일: rtp_depacketizer.h
// 생성일: 2026-02-23
// 설명: RTP 디패킷타이저 인터페이스

#pragma once

#include "nxnet/rtp/rtp_types.h"
#include <span>
#include <vector>
#include <cstdint>
#include <functional>

namespace nx::rtp {

// RTP 디패킷타이저 인터페이스
class RtpDepacketizer
{
public:
  virtual ~RtpDepacketizer() = default;

  // RTP 패킷 처리
  // 프레임이 완성되면 true 반환, out_frame에 완성된 프레임 데이터 저장
  virtual bool process_packet(
    const RtpHeaderView& header,
    std::span<const uint8_t> payload,
    std::vector<uint8_t>& out_frame,
    bool& out_keyframe) = 0;

  // 상태 초기화
  virtual void reset() = 0;

  // 코덱 불일치 감지 콜백
  // 연속된 비정상 NAL 유닛이 임계치를 초과하면 호출
  using CodecMismatchCallback = std::function<void(uint32_t consecutive_errors)>;

  void set_codec_mismatch_callback(CodecMismatchCallback callback)
  {
    m_codec_mismatch_callback = std::move(callback);
  }

  // 연속 오류 카운트 조회
  uint32_t consecutive_error_count() const { return m_consecutive_errors; }

protected:
  // 하위 클래스에서 비정상 NAL/패킷 감지 시 호출
  // rtp_timestamp: 해당 패킷의 RTP 타임스탬프 (프레임 단위 구분)
  void report_unknown_nal(uint8_t, uint32_t rtp_timestamp)
  {
    ++m_consecutive_errors;

    // 서로 다른 RTP 타임스탬프(프레임)에서 연속 에러가 발생한 횟수 추적
    if (rtp_timestamp != m_last_error_timestamp) {
      m_last_error_timestamp = rtp_timestamp;
      ++m_error_frame_count;
    }

    if (
      !m_mismatch_reported && m_error_frame_count >= kMismatchFrameThreshold
      && m_codec_mismatch_callback) {
      m_mismatch_reported = true;
      m_codec_mismatch_callback(m_error_frame_count);
    }
  }

  // 명시적 상태 초기화 시 오류 카운트 리셋 (reset()에서 호출)
  // 주의: 유효 NAL 수신 시 호출하면 코덱 변경 감지가 무력화됨
  void reset_error_count()
  {
    m_consecutive_errors = 0;
    m_error_frame_count = 0;
    m_last_error_timestamp = 0;
    m_mismatch_reported = false;
  }

private:
  CodecMismatchCallback m_codec_mismatch_callback;
  uint32_t m_consecutive_errors = 0;
  uint32_t m_error_frame_count = 0;
  uint32_t m_last_error_timestamp = 0;

  // 서로 다른 N개 프레임에서 연속 에러 시 콜백 발화 (한 번만)
  static constexpr uint32_t kMismatchFrameThreshold = 3;
  bool m_mismatch_reported = false;
};

} // namespace nx::rtp
