// 파일: rtsp_session.h
// 생성일: 2026-02-23
// 설명: RTSP 세션 상태 관리

#pragma once

#include "nxnet/rtsp/rtsp_types.h"
#include "nxnet/sdp/sdp_session.h"

#include <nxcore/util/time_util.h>
#include <optional>
#include <string>
#include <vector>

namespace nx::net {

class RtspSession
{
public:
  RtspSession() = default;

  // 세션 ID 관리
  void set_session_id(const std::string& id) { m_session_id = id; }
  const std::string& session_id() const noexcept { return m_session_id; }

  // 상태 관리
  void set_state(RtspSessionState state) { m_state = state; }
  RtspSessionState state() const noexcept { return m_state; }

  // SDP 관리
  void set_sdp(sdp::SdpSession sdp) { m_sdp = std::move(sdp); }
  const std::optional<sdp::SdpSession>& sdp() const noexcept { return m_sdp; }

  // 전송 정보 관리
  void add_transport(RtspTransportInfo info) { m_transports.push_back(std::move(info)); }
  const std::vector<RtspTransportInfo>& transports() const noexcept
  {
    return m_transports;
  }

  // 세션 타임아웃 관리
  void set_timeout(nx::seconds timeout) { m_timeout = timeout; }
  nx::seconds timeout() const noexcept { return m_timeout; }

  // CSeq 관리
  [[nodiscard]]
  uint32_t next_cseq() noexcept
  {
    return ++m_cseq;
  }
  uint32_t current_cseq() const noexcept { return m_cseq; }

  // 초기화
  void reset()
  {
    m_session_id.clear();
    m_state = RtspSessionState::kDisconnected;
    m_sdp.reset();
    m_transports.clear();
    m_cseq = 0;
  }

private:
  std::string m_session_id;
  RtspSessionState m_state = RtspSessionState::kDisconnected;
  std::optional<sdp::SdpSession> m_sdp;
  std::vector<RtspTransportInfo> m_transports;
  nx::seconds m_timeout{60};
  uint32_t m_cseq = 0;
};

} // namespace nx::net
