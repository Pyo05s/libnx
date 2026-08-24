// 파일: rtsp_message.h
// 생성일: 2026-02-23
// 설명: RTSP 메시지 구조 (요청 및 응답)

#pragma once

#include "nxnet/rtsp/rtsp_types.h"
#include <map>
#include <string>
#include <cstdint>

namespace nx::net {

// RTSP 요청 메시지
struct RtspRequest
{
  RtspMethod method = RtspMethod::kOptions;
  std::string uri;
  std::string version = "RTSP/1.0";
  std::map<std::string, std::string> headers;
  std::string body;
  uint32_t cseq = 0;

  // 요청 문자열 직렬화
  std::string serialize() const;
};

// RTSP 응답 메시지
struct RtspResponse
{
  std::string version;
  uint16_t status_code = 0;
  std::string reason_phrase;
  std::map<std::string, std::string> headers;
  std::string body;
  uint32_t cseq = 0;

  // 성공 여부
  bool is_success() const noexcept { return status_code >= 200 && status_code < 300; }

  // 특정 헤더 검색 (대소문자 무시)
  std::string find_header(const std::string& name) const;
};

} // namespace nx::net
