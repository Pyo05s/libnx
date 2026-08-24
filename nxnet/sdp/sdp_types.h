// 파일: sdp_types.h
// 생성일: 2026-02-23
// 설명: SDP 공통 타입 정의

#pragma once

#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

namespace nx::sdp {

// SDP 미디어 타입
enum class SdpMediaType
{
  kAudio,
  kVideo,
  kText,
  kApplication,
  kMessage
};

// SDP 미디어 디스크립션
struct SdpMedia
{
  SdpMediaType type = SdpMediaType::kVideo;
  uint16_t port = 0;
  std::string protocol;     // "RTP/AVP", "RTP/SAVP"
  std::vector<int> formats; // Payload type 목록

  std::string connection_address; // 미디어 레벨 연결 주소

  // RTP 매핑: PT -> "codec/clockrate"
  std::map<int, std::string> rtpmap; // 예: 96 -> "H264/90000"

  // 포맷 파라미터: PT -> params
  std::map<int, std::string> fmtp; // 예: 96 -> "packetization-mode=1;..."

  // Control URL (RTSP SETUP용)
  std::string control_url;

  std::optional<double> framerate;

  // 기타 속성
  std::map<std::string, std::string> attributes;
};

// SDP Origin 정보
struct SdpOrigin
{
  std::string username = "-";
  uint64_t session_id = 0;
  uint64_t session_version = 0;
  std::string net_type = "IN";
  std::string addr_type = "IP4";
  std::string address;
};

} // namespace nx::sdp
