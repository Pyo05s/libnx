// 파일: sdp_session.h
// 생성일: 2026-02-23
// 설명: SDP 세션 표현

#pragma once

#include "nxnet/sdp/sdp_types.h"
#include <string>
#include <vector>

namespace nx::sdp {

class SdpSession
{
public:
  SdpSession() = default;

  // ========================================================================
  // 세션 레벨 설정
  // ========================================================================

  void set_session_name(const std::string& name) { m_session_name = name; }

  void set_origin(const SdpOrigin& origin) { m_origin = origin; }

  void set_connection(const std::string& address) { m_connection_address = address; }

  void set_timing(uint64_t start, uint64_t stop)
  {
    m_start_time = start;
    m_stop_time = stop;
  }

  void set_base_url(const std::string& url) { m_base_url = url; }

  // ========================================================================
  // 미디어 관리
  // ========================================================================

  void add_media(SdpMedia media) { m_media_list.push_back(std::move(media)); }

  // ========================================================================
  // 조회
  // ========================================================================

  const std::string& session_name() const noexcept { return m_session_name; }

  const SdpOrigin& origin() const noexcept { return m_origin; }

  const std::string& connection_address() const noexcept
  {
    return m_connection_address;
  }

  const std::string& base_url() const noexcept { return m_base_url; }

  const std::vector<SdpMedia>& media_descriptions() const noexcept
  {
    return m_media_list;
  }

  // 특정 타입의 미디어 검색
  const SdpMedia* find_media(SdpMediaType type) const noexcept
  {
    for (const auto& media : m_media_list) {
      if (media.type == type) {
        return &media;
      }
    }
    return nullptr;
  }

  SdpMedia* find_media(SdpMediaType type) noexcept
  {
    for (auto& media : m_media_list) {
      if (media.type == type) {
        return &media;
      }
    }
    return nullptr;
  }

  bool has_video() const noexcept
  {
    return find_media(SdpMediaType::kVideo) != nullptr;
  }

  bool has_audio() const noexcept
  {
    return find_media(SdpMediaType::kAudio) != nullptr;
  }

private:
  std::string m_session_name;
  SdpOrigin m_origin;
  std::string m_connection_address;
  std::string m_base_url;
  uint64_t m_start_time = 0;
  uint64_t m_stop_time = 0;
  std::vector<SdpMedia> m_media_list;
};

} // namespace nx::sdp
