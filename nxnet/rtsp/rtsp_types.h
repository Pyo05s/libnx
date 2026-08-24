// 파일: rtsp_types.h
// 생성일: 2026-02-23
// 설명: RTSP 공통 타입 정의

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nx::net {

// RTSP 전송 모드
enum class RtspTransport
{
  kRtpUdp,         // RTP over UDP (표준)
  kRtpTcp,         // RTP over TCP (Interleaved)
  kRtpHttp,        // RTP over HTTP (터널링)
  kRtpUdpMulticast // RTP over UDP Multicast
};

// RTSP 메서드
enum class RtspMethod
{
  kOptions,
  kDescribe,
  kSetup,
  kPlay,
  kPause,
  kTeardown,
  kGetParameter,
  kSetParameter
};

// RTSP 세션 상태
enum class RtspSessionState
{
  kDisconnected,
  kConnected,
  kDescribed,
  kReady,
  kPlaying,
  kPaused
};

// RTSP 전송 정보 (SETUP 응답에서 파싱)
struct RtspTransportInfo
{
  RtspTransport transport = RtspTransport::kRtpTcp;
  std::string server_ip;
  uint16_t rtp_port = 0;
  uint16_t rtcp_port = 0;
  std::optional<uint16_t> client_rtp_port;
  std::optional<uint16_t> client_rtcp_port;
  std::optional<std::string> ssrc;
  bool unicast = true;

  // TCP Interleaved 모드
  std::optional<uint8_t> interleaved_rtp;
  std::optional<uint8_t> interleaved_rtcp;
};

// RTSP 메서드 -> 문자열 변환
inline const char*
rtsp_method_to_string(RtspMethod method)
{
  switch (method) {
    case RtspMethod::kOptions: return "OPTIONS";
    case RtspMethod::kDescribe: return "DESCRIBE";
    case RtspMethod::kSetup: return "SETUP";
    case RtspMethod::kPlay: return "PLAY";
    case RtspMethod::kPause: return "PAUSE";
    case RtspMethod::kTeardown: return "TEARDOWN";
    case RtspMethod::kGetParameter: return "GET_PARAMETER";
    case RtspMethod::kSetParameter: return "SET_PARAMETER";
    default: return "UNKNOWN";
  }
}

} // namespace nx::net
