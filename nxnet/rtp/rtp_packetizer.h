// 파일: rtp_packetizer.h
// 생성일: 2026-02-26
// 설명: RTP 패킷타이저 인터페이스

#pragma once

#include "nxnet/rtp/rtp_types.h"
#include "nxnet/rtp/rtp_header_extension.h"
#include "nxnet/rtp/rtp_frame_buffer.h"

#include <cstdint>
#include <span>
#include <vector>

namespace nx::rtp {

/// RTP 패킷타이저 인터페이스
/// - 인코딩된 프레임 데이터를 RTP 패킷으로 분할/집합
/// - 결과를 RtpFrameBuffer에 직접 기록 (힙 할당 최소화)
/// - 코덱별 서브클래스에서 구현
class RtpPacketizer
{
public:
  virtual ~RtpPacketizer() = default;

  /// 프레임 데이터를 RTP 패킷으로 변환하여 RtpFrameBuffer에 직접 기록
  /// @param frame_data 인코딩된 프레임 (Annex B 또는 raw NAL)
  /// @param timestamp RTP 타임스탬프 (90kHz 클럭)
  /// @param is_keyframe 키프레임 여부
  /// @param output 출력 버퍼 (clear() 완료 상태)
  virtual void packetize(
    std::span<const uint8_t> frame_data,
    uint32_t timestamp,
    bool is_keyframe,
    RtpFrameBuffer& output) = 0;

  /// 프레임 데이터를 RTP 패킷으로 변환 (RFC 5285 헤더 확장 포함)
  /// @param frame_data 인코딩된 프레임
  /// @param timestamp RTP 타임스탬프
  /// @param is_keyframe 키프레임 여부
  /// @param ext 확장 헤더 빌더 (각 패킷에 동일 확장 삽입)
  /// @param output 출력 버퍼 (clear() 완료 상태)
  virtual void packetize(
    std::span<const uint8_t> frame_data,
    uint32_t timestamp,
    bool is_keyframe,
    const RtpHeaderExtensionBuilder& ext,
    RtpFrameBuffer& output);

  /// 상태 초기화
  virtual void reset() = 0;

  /// SSRC 설정
  virtual void set_ssrc(uint32_t ssrc) = 0;

  /// 페이로드 타입 설정
  virtual void set_payload_type(uint8_t pt) = 0;

  /// 최대 RTP 패킷 크기 설정 (기본 1400)
  virtual void set_max_packet_size(uint16_t size) = 0;
};

} // namespace nx::rtp
