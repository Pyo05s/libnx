// 파일: rtsp_parser.h
// 생성일: 2026-02-23
// 설명: RTSP 응답 파서 및 Transport 헤더 파서

#pragma once

#include "nxcore/util/type_util.h"
#include "nxnet/rtsp/rtsp_message.h"
#include "nxnet/rtsp/rtsp_error.h"

#include <nxcore/util/type_util.h>

#include <expected>
#include <string_view>
#include <system_error>

namespace nx::net {

class RtspParser
{
public:
  NX_NON_INSTANTIABLE(RtspParser);

  // RTSP 응답 파싱
  static nx::expected<RtspResponse> parse_response(std::string_view data);

  // Transport 헤더 파싱
  static nx::expected<RtspTransportInfo>
  parse_transport(std::string_view transport_header);

  // RTSP 응답이 완전한지 확인 (헤더 + 본문)
  // 반환값: 완전한 응답의 바이트 수 (0이면 불완전)
  static size_t find_response_end(std::string_view data);

private:
  // 유틸리티
  static std::string_view trim(std::string_view str);
};

} // namespace nx::net
