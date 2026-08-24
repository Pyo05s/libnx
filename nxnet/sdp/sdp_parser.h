// 파일: sdp_parser.h
// 생성일: 2026-02-23
// 설명: SDP 파서

#pragma once

#include "nxcore/util/type_util.h"
#include "nxnet/sdp/sdp_session.h"
#include "nxnet/sdp/sdp_error.h"

#include <expected>
#include <string_view>
#include <system_error>

namespace nx::sdp {

class SdpParser
{
public:
  NX_NON_INSTANTIABLE(SdpParser);

  // SDP 텍스트를 파싱하여 SdpSession 객체 생성
  static nx::expected<SdpSession> parse(std::string_view sdp_text);

private:
  // 세션 레벨 라인 파싱
  static std::error_code parse_version_line(std::string_view value, SdpSession& session);
  static std::error_code parse_origin_line(std::string_view value, SdpSession& session);
  static std::error_code
  parse_session_name_line(std::string_view value, SdpSession& session);
  static std::error_code parse_connection_line(
    std::string_view value, SdpSession& session, SdpMedia* current_media);
  static std::error_code parse_timing_line(std::string_view value, SdpSession& session);

  // 미디어 라인 파싱
  static nx::expected<SdpMedia> parse_media_line(std::string_view value);

  // 속성 라인 파싱
  static std::error_code parse_attribute_line(
    std::string_view value, SdpSession& session, SdpMedia* current_media);

  // 미디어 타입 문자열 변환
  static std::optional<SdpMediaType> parse_media_type(std::string_view type_str);

  // 유틸리티
  static std::string_view trim(std::string_view str);
};

} // namespace nx::sdp
