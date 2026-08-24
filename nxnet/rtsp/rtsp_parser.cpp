// 파일: rtsp_parser.cpp
// 생성일: 2026-02-23
// 설명: RTSP 응답 파서 구현

#include "rtsp_parser.h"

#include <algorithm>
#include <charconv>
#include <sstream>

namespace nx::net {

nx::expected<RtspResponse>
RtspParser::parse_response(std::string_view data)
{
  if (data.empty()) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }

  RtspResponse response;

  // 헤더와 본문 분리
  auto header_end = data.find("\r\n\r\n");
  if (header_end == std::string_view::npos) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }

  auto header_section = data.substr(0, header_end);
  auto body_start = header_end + 4;

  // 상태 라인 파싱: "RTSP/1.0 200 OK"
  auto first_line_end = header_section.find("\r\n");
  if (first_line_end == std::string_view::npos) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }

  auto status_line = header_section.substr(0, first_line_end);

  // 버전 파싱
  auto space1 = status_line.find(' ');
  if (space1 == std::string_view::npos) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }
  response.version = std::string(status_line.substr(0, space1));

  // 상태 코드 파싱
  auto rest = status_line.substr(space1 + 1);
  auto space2 = rest.find(' ');
  if (space2 == std::string_view::npos) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }

  auto code_str = rest.substr(0, space2);
  auto [ptr, ec] = std::from_chars(
    code_str.data(),
    code_str.data() + code_str.size(),
    response.status_code);
  if (ec != std::errc{}) {
    return std::unexpected(make_error_code(RtspErrc::invalid_response));
  }

  // 사유 문구
  response.reason_phrase = std::string(rest.substr(space2 + 1));

  // 헤더 파싱
  auto headers_start = first_line_end + 2; // "\r\n" 이후
  auto remaining_headers = header_section.substr(headers_start);

  std::istringstream header_stream{std::string{remaining_headers}};
  std::string header_line;

  while (std::getline(header_stream, header_line)) {
    // CR 제거
    if (!header_line.empty() && header_line.back() == '\r') {
      header_line.pop_back();
    }

    if (header_line.empty()) {
      continue;
    }

    auto colon_pos = header_line.find(':');
    if (colon_pos == std::string::npos) {
      continue;
    }

    auto key = std::string(trim(std::string_view(header_line).substr(0, colon_pos)));
    auto value = std::string(trim(std::string_view(header_line).substr(colon_pos + 1)));

    response.headers[key] = value;
  }

  // CSeq 추출
  auto cseq_it = response.headers.find("CSeq");
  if (cseq_it == response.headers.end()) {
    // 대소문자 무시 검색
    for (const auto& [key, value] : response.headers) {
      auto key_lower = key;
      std::transform(
        key_lower.begin(),
        key_lower.end(),
        key_lower.begin(),
        [](unsigned char c) { return std::tolower(c); });
      if (key_lower == "cseq") {
        uint32_t cseq_val = 0;
        auto [p, e]
          = std::from_chars(value.data(), value.data() + value.size(), cseq_val);
        if (e == std::errc{}) {
          response.cseq = cseq_val;
        }
        break;
      }
    }
  }
  else {
    uint32_t cseq_val = 0;
    auto [p, e] = std::from_chars(
      cseq_it->second.data(),
      cseq_it->second.data() + cseq_it->second.size(),
      cseq_val);
    if (e == std::errc{}) {
      response.cseq = cseq_val;
    }
  }

  // 본문 추출
  if (body_start < data.size()) {
    // Content-Length 확인
    auto content_length_str = response.find_header("Content-Length");
    if (!content_length_str.empty()) {
      size_t content_length = 0;
      auto [p, e] = std::from_chars(
        content_length_str.data(),
        content_length_str.data() + content_length_str.size(),
        content_length);
      if (e == std::errc{} && body_start + content_length <= data.size()) {
        response.body = std::string(data.substr(body_start, content_length));
      }
    }
    else {
      response.body = std::string(data.substr(body_start));
    }
  }

  return response;
}

nx::expected<RtspTransportInfo>
RtspParser::parse_transport(std::string_view transport_header)
{
  // Transport: RTP/AVP/TCP;unicast;interleaved=0-1
  // Transport: RTP/AVP;unicast;client_port=50000-50001;server_port=60000-60001

  if (transport_header.empty()) {
    return std::unexpected(make_error_code(RtspErrc::invalid_transport));
  }

  RtspTransportInfo info;

  // 세미콜론으로 분리
  std::istringstream ss{std::string{transport_header}};
  std::string token;
  bool first = true;

  while (std::getline(ss, token, ';')) {
    auto trimmed = trim(std::string_view(token));

    if (first) {
      first = false;
      // 전송 프로토콜 파싱
      if (trimmed.find("RTP/AVP/TCP") != std::string_view::npos) {
        info.transport = RtspTransport::kRtpTcp;
      }
      else if (trimmed.find("RTP/AVP") != std::string_view::npos) {
        info.transport = RtspTransport::kRtpUdp;
      }
      continue;
    }

    if (trimmed == "unicast") {
      info.unicast = true;
    }
    else if (trimmed == "multicast") {
      info.unicast = false;
      info.transport = RtspTransport::kRtpUdpMulticast;
    }
    else if (trimmed.starts_with("interleaved=")) {
      auto val = trimmed.substr(12);
      auto dash = val.find('-');
      if (dash != std::string_view::npos) {
        uint8_t rtp_ch = 0;
        uint8_t rtcp_ch = 0;
        auto rtp_str = val.substr(0, dash);
        auto rtcp_str = val.substr(dash + 1);
        std::from_chars(rtp_str.data(), rtp_str.data() + rtp_str.size(), rtp_ch);
        std::from_chars(rtcp_str.data(), rtcp_str.data() + rtcp_str.size(), rtcp_ch);
        info.interleaved_rtp = rtp_ch;
        info.interleaved_rtcp = rtcp_ch;
      }
    }
    else if (trimmed.starts_with("client_port=")) {
      auto val = trimmed.substr(12);
      auto dash = val.find('-');
      if (dash != std::string_view::npos) {
        uint16_t rtp_port = 0;
        uint16_t rtcp_port = 0;
        auto rtp_str = val.substr(0, dash);
        auto rtcp_str = val.substr(dash + 1);
        std::from_chars(rtp_str.data(), rtp_str.data() + rtp_str.size(), rtp_port);
        std::from_chars(rtcp_str.data(), rtcp_str.data() + rtcp_str.size(), rtcp_port);
        info.client_rtp_port = rtp_port;
        info.client_rtcp_port = rtcp_port;
      }
    }
    else if (trimmed.starts_with("server_port=")) {
      auto val = trimmed.substr(12);
      auto dash = val.find('-');
      if (dash != std::string_view::npos) {
        auto rtp_str = val.substr(0, dash);
        auto rtcp_str = val.substr(dash + 1);
        std::from_chars(rtp_str.data(), rtp_str.data() + rtp_str.size(), info.rtp_port);
        std::from_chars(
          rtcp_str.data(),
          rtcp_str.data() + rtcp_str.size(),
          info.rtcp_port);
      }
    }
    else if (trimmed.starts_with("ssrc=")) {
      info.ssrc = std::string(trimmed.substr(5));
    }
    else if (trimmed.starts_with("source=")) {
      info.server_ip = std::string(trimmed.substr(7));
    }
  }

  return info;
}

size_t
RtspParser::find_response_end(std::string_view data)
{
  // 헤더 끝 찾기
  auto header_end = data.find("\r\n\r\n");
  if (header_end == std::string_view::npos) {
    return 0; // 불완전한 응답
  }

  size_t body_start = header_end + 4;

  // Content-Length 헤더 찾기
  auto cl_pos = data.find("Content-Length:");
  if (cl_pos == std::string_view::npos) {
    cl_pos = data.find("content-length:");
  }

  if (cl_pos == std::string_view::npos || cl_pos > header_end) {
    // Content-Length 없음 -> 헤더만 있는 응답
    return body_start;
  }

  // Content-Length 값 파싱
  auto cl_value_start = cl_pos + 15; // "Content-Length:" 길이
  while (cl_value_start < header_end && data[cl_value_start] == ' ') {
    ++cl_value_start;
  }

  auto cl_value_end = data.find("\r\n", cl_value_start);
  if (cl_value_end == std::string_view::npos) {
    return 0;
  }

  size_t content_length = 0;
  auto cl_str = data.substr(cl_value_start, cl_value_end - cl_value_start);
  auto [ptr, ec]
    = std::from_chars(cl_str.data(), cl_str.data() + cl_str.size(), content_length);

  if (ec != std::errc{}) {
    return body_start;
  }

  size_t total_length = body_start + content_length;
  if (data.size() < total_length) {
    return 0; // 본문이 아직 완전히 수신되지 않음
  }

  return total_length;
}

std::string_view
RtspParser::trim(std::string_view str)
{
  auto start = str.find_first_not_of(" \t");
  if (start == std::string_view::npos) {
    return {};
  }
  auto end = str.find_last_not_of(" \t");
  return str.substr(start, end - start + 1);
}

} // namespace nx::net
