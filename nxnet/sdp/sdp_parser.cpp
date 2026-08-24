// 파일: sdp_parser.cpp
// 생성일: 2026-02-23
// 설명: SDP 파서 구현

#include "sdp_parser.h"

#include <charconv>
#include <sstream>
#include <algorithm>

namespace nx::sdp {

nx::expected<SdpSession>
SdpParser::parse(std::string_view sdp_text)
{
  if (sdp_text.empty()) {
    return std::unexpected(make_error_code(SdpErrc::invalid_format));
  }

  SdpSession session;
  SdpMedia* current_media = nullptr;
  bool has_version = false;

  // 라인 단위 파싱
  std::istringstream stream{std::string{sdp_text}};
  std::string line;

  while (std::getline(stream, line)) {
    // CR 제거
    if (!line.empty() && line.back() == '\r') {
      line.pop_back();
    }

    if (line.empty()) {
      continue;
    }

    // 최소 형식: "x=..."
    if (line.size() < 2 || line[1] != '=') {
      continue;
    }

    char type = line[0];
    auto value = trim(std::string_view(line).substr(2));

    std::error_code ec;

    switch (type) {
      case 'v':
        ec = parse_version_line(value, session);
        if (ec) {
          return std::unexpected(ec);
        }
        has_version = true;
        break;

      case 'o': ec = parse_origin_line(value, session); break;

      case 's': ec = parse_session_name_line(value, session); break;

      case 'c': ec = parse_connection_line(value, session, current_media); break;

      case 't': ec = parse_timing_line(value, session); break;

      case 'm': {
        auto media_result = parse_media_line(value);
        if (!media_result) {
          return std::unexpected(media_result.error());
        }
        session.add_media(std::move(*media_result));
        // 마지막으로 추가된 미디어를 현재 미디어로 설정
        auto& media_list
          = const_cast<std::vector<SdpMedia>&>(session.media_descriptions());
        current_media = &media_list.back();
        break;
      }

      case 'a': ec = parse_attribute_line(value, session, current_media); break;

      default:
        // 알 수 없는 라인은 무시
        break;
    }

    if (ec) {
      return std::unexpected(ec);
    }
  }

  if (!has_version) {
    return std::unexpected(make_error_code(SdpErrc::missing_version));
  }

  return session;
}

std::error_code
SdpParser::parse_version_line(std::string_view value, SdpSession& /*session*/)
{
  if (value != "0") {
    return make_error_code(SdpErrc::unsupported_version);
  }
  return {};
}

std::error_code
SdpParser::parse_origin_line(std::string_view value, SdpSession& session)
{
  // o=<username> <sess-id> <sess-version> <nettype> <addrtype> <unicast-address>
  SdpOrigin origin;

  std::istringstream ss{std::string{value}};
  std::string token;

  if (!(ss >> origin.username)) {
    return make_error_code(SdpErrc::missing_origin);
  }
  if (!(ss >> origin.session_id)) {
    return make_error_code(SdpErrc::missing_origin);
  }
  if (!(ss >> origin.session_version)) {
    return make_error_code(SdpErrc::missing_origin);
  }
  if (!(ss >> origin.net_type)) {
    return make_error_code(SdpErrc::missing_origin);
  }
  if (!(ss >> origin.addr_type)) {
    return make_error_code(SdpErrc::missing_origin);
  }
  if (!(ss >> origin.address)) {
    return make_error_code(SdpErrc::missing_origin);
  }

  session.set_origin(origin);
  return {};
}

std::error_code
SdpParser::parse_session_name_line(std::string_view value, SdpSession& session)
{
  session.set_session_name(std::string(value));
  return {};
}

std::error_code
SdpParser::parse_connection_line(
  std::string_view value, SdpSession& session, SdpMedia* current_media)
{
  // c=IN IP4 <address>
  std::istringstream ss{std::string{value}};
  std::string net_type;
  std::string addr_type;
  std::string address;

  if (!(ss >> net_type >> addr_type >> address)) {
    return make_error_code(SdpErrc::invalid_connection);
  }

  // 멀티캐스트 주소에서 TTL 제거 (예: "239.255.0.1/127")
  auto slash_pos = address.find('/');
  if (slash_pos != std::string::npos) {
    address = address.substr(0, slash_pos);
  }

  if (current_media) {
    current_media->connection_address = address;
  }
  else {
    session.set_connection(address);
  }

  return {};
}

std::error_code
SdpParser::parse_timing_line(std::string_view value, SdpSession& session)
{
  // t=<start-time> <stop-time>
  uint64_t start = 0;
  uint64_t stop = 0;

  std::istringstream ss{std::string{value}};
  if (!(ss >> start >> stop)) {
    return make_error_code(SdpErrc::invalid_timing);
  }

  session.set_timing(start, stop);
  return {};
}

nx::expected<SdpMedia>
SdpParser::parse_media_line(std::string_view value)
{
  // m=<media> <port> <proto> <fmt> ...
  std::istringstream ss{std::string{value}};

  std::string media_type_str;
  uint16_t port = 0;
  std::string protocol;

  if (!(ss >> media_type_str >> port >> protocol)) {
    return std::unexpected(make_error_code(SdpErrc::invalid_media_line));
  }

  auto media_type = parse_media_type(media_type_str);
  if (!media_type) {
    return std::unexpected(make_error_code(SdpErrc::invalid_media_line));
  }

  SdpMedia media;
  media.type = *media_type;
  media.port = port;
  media.protocol = protocol;

  // Payload type 목록 파싱
  int fmt = 0;
  while (ss >> fmt) {
    media.formats.push_back(fmt);
  }

  return media;
}

std::error_code
SdpParser::parse_attribute_line(
  std::string_view value, SdpSession& session, SdpMedia* current_media)
{
  // a=<attribute> 또는 a=<attribute>:<value>
  auto colon_pos = value.find(':');

  std::string attr_name;
  std::string attr_value;

  if (colon_pos != std::string_view::npos) {
    attr_name = std::string(value.substr(0, colon_pos));
    attr_value = std::string(value.substr(colon_pos + 1));
  }
  else {
    attr_name = std::string(value);
  }

  // 세션 레벨 속성
  if (!current_media) {
    if (attr_name == "control") {
      session.set_base_url(attr_value);
    }
    return {};
  }

  // 미디어 레벨 속성
  if (attr_name == "rtpmap") {
    // a=rtpmap:<payload type> <encoding>/<clock rate>[/<channels>]
    auto space_pos = attr_value.find(' ');
    if (space_pos != std::string::npos) {
      int pt = 0;
      auto pt_str = attr_value.substr(0, space_pos);
      auto [ptr, ec] = std::from_chars(pt_str.data(), pt_str.data() + pt_str.size(), pt);
      if (ec == std::errc{}) {
        current_media->rtpmap[pt] = attr_value.substr(space_pos + 1);
      }
    }
  }
  else if (attr_name == "fmtp") {
    // a=fmtp:<format> <format specific parameters>
    auto space_pos = attr_value.find(' ');
    if (space_pos != std::string::npos) {
      int pt = 0;
      auto pt_str = attr_value.substr(0, space_pos);
      auto [ptr, ec] = std::from_chars(pt_str.data(), pt_str.data() + pt_str.size(), pt);
      if (ec == std::errc{}) {
        current_media->fmtp[pt] = attr_value.substr(space_pos + 1);
      }
    }
  }
  else if (attr_name == "control") {
    current_media->control_url = attr_value;
  }
  else if (attr_name == "framerate") {
    try {
      current_media->framerate = std::stod(attr_value);
    }
    catch (...) {
      // 프레임레이트 파싱 실패는 무시
    }
  }
  else {
    current_media->attributes[attr_name] = attr_value;
  }

  return {};
}

std::optional<SdpMediaType>
SdpParser::parse_media_type(std::string_view type_str)
{
  if (type_str == "video") {
    return SdpMediaType::kVideo;
  }
  if (type_str == "audio") {
    return SdpMediaType::kAudio;
  }
  if (type_str == "text") {
    return SdpMediaType::kText;
  }
  if (type_str == "application") {
    return SdpMediaType::kApplication;
  }
  if (type_str == "message") {
    return SdpMediaType::kMessage;
  }
  return std::nullopt;
}

std::string_view
SdpParser::trim(std::string_view str)
{
  auto start = str.find_first_not_of(" \t");
  if (start == std::string_view::npos) {
    return {};
  }
  auto end = str.find_last_not_of(" \t");
  return str.substr(start, end - start + 1);
}

} // namespace nx::sdp
