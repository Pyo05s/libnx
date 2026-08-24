// 파일: uri_util.h
// 생성일: 2026-02-17
// 설명: URI 파싱 및 조작 유틸리티

#pragma once

#include <string>
#include <optional>
#include <cstdint>

namespace nx {

// ============================================================================
// URI 구성 요소
// ============================================================================

/// URI 파싱 결과
struct UriComponents
{
  std::string scheme;   // 프로토콜 (http, rtsp 등)
  std::string username; // 사용자 ID (선택 사항)
  std::string password; // 사용자 비밀번호 (선택 사항)
  std::string host;     // 호스트 (IP 또는 도메인)
  uint16_t port{0};     // 포트 번호
  std::string path;     // 경로 (/onvif/device_service 등)
  std::string query;    // 쿼리 문자열 (선택 사항)
  std::string fragment; // 프래그먼트 (선택 사항)
};

// ============================================================================
// URI 파싱 및 생성
// ============================================================================

/// URI 문자열 파싱
/// 예: "http://192.168.0.168:80/onvif/device_service"
std::optional<UriComponents> parse_uri(const std::string& uri);

/// URI 구성 요소에서 URI 문자열 생성
std::string build_uri(const UriComponents& components);

/// URI에서 호스트와 포트를 교체
/// 예: "rtsp://192.168.0.100:554/stream1" + ("192.168.0.168", 554)
///     -> "rtsp://192.168.0.168:554/stream1"
std::string replace_uri_authority(
  const std::string& uri, const std::string& new_host, uint16_t new_port);

/// URI에서 호스트만 교체 (포트는 유지)
std::string replace_uri_host(const std::string& uri, const std::string& new_host);

/// URI에서 포트만 교체 (호스트는 유지)
std::string replace_uri_port(const std::string& uri, uint16_t new_port);

/// URL 인코딩
std::string url_encode(const std::string& str);

/// URL 디코딩
std::string url_decode(const std::string& str);

} // namespace nx
