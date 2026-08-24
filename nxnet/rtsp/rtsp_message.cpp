// 파일: rtsp_message.cpp
// 생성일: 2026-02-23
// 설명: RTSP 메시지 직렬화 및 유틸리티 구현

#include "rtsp_message.h"

#include <algorithm>
#include <sstream>

namespace nx::net {

std::string
RtspRequest::serialize() const
{
  std::ostringstream oss;

  // 요청 라인: METHOD URI RTSP-Version
  oss << rtsp_method_to_string(method) << " " << uri << " " << version << "\r\n";

  // CSeq 헤더
  oss << "CSeq: " << cseq << "\r\n";

  // 사용자 지정 헤더
  for (const auto& [key, value] : headers) {
    // CSeq는 이미 추가했으므로 건너뜀
    if (key == "CSeq") {
      continue;
    }
    oss << key << ": " << value << "\r\n";
  }

  // Content-Length (본문이 있는 경우)
  if (!body.empty()) {
    oss << "Content-Length: " << body.size() << "\r\n";
  }

  // 빈 줄 (헤더 종료)
  oss << "\r\n";

  // 본문
  if (!body.empty()) {
    oss << body;
  }

  return oss.str();
}

std::string
RtspResponse::find_header(const std::string& name) const
{
  // 대소문자 무시 검색
  auto name_lower = name;
  std::transform(
    name_lower.begin(),
    name_lower.end(),
    name_lower.begin(),
    [](unsigned char c) { return std::tolower(c); });

  for (const auto& [key, value] : headers) {
    auto key_lower = key;
    std::transform(
      key_lower.begin(),
      key_lower.end(),
      key_lower.begin(),
      [](unsigned char c) { return std::tolower(c); });
    if (key_lower == name_lower) {
      return value;
    }
  }
  return {};
}

} // namespace nx::net
