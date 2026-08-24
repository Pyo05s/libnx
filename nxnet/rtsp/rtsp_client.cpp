// 파일: rtsp_client.cpp
// 생성일: 2026-02-23
// 설명: 메인 RTSP 클라이언트 구현

#include "rtsp_client.h"
#include "rtsp_parser.h"

#include "nxnet/auth/auth_challenge_parser.h"
#include "nxnet/auth/auth_provider.h"
#include "nxnet/sdp/sdp_parser.h"
#include "nxnet/rtp/rtp_h264_depacketizer.h"
#include "nxnet/rtp/rtp_h265_depacketizer.h"
#include "nxnet/rtp/rtp_jpeg_depacketizer.h"
#include "nxcore/media/media_codec.h"
#include "nxcore/util/uri_util.h"
#include "nxcore/util/scoped_perf_timer.h"

#include <spdlog/spdlog.h>

namespace nx::net {

RtspClient::RtspClient(
  AsioContext& ioc, uint32_t connect_timeout_ms, uint32_t response_timeout_ms)
    : m_ioc(ioc)
    , m_connect_timeout(connect_timeout_ms)
    , m_response_timeout(response_timeout_ms)
{}

RtspClient::~RtspClient()
{
  m_receiving.store(false);
  if (m_receive_loop_active.load()) {
    spdlog::critical("RtspClient 소멸 시 수신 루프가 아직 활성 상태");
  }
}

nx::awaitable<std::error_code>
RtspClient::connect(const std::string& rtsp_url)
{
  if (m_session.state() != RtspSessionState::kDisconnected) {
    co_return make_error_code(RtspErrc::already_connected);
  }

  // URL 파싱
  auto components = nx::parse_uri(rtsp_url);
  if (!components) {
    co_return make_error_code(RtspErrc::invalid_url);
  }

  m_rtsp_url = rtsp_url;
  m_host = components->host;
  m_port = components->port != 0 ? components->port : 554;

  // TCP 연결
  m_connection
    = std::make_unique<RtspConnection>(m_ioc, m_connect_timeout, m_response_timeout);
  auto ec = co_await m_connection->connect(m_host, m_port);
  if (ec) {
    co_return ec;
  }

  m_session.set_state(RtspSessionState::kConnected);
  spdlog::info("RTSP 연결 성공: {}", rtsp_url);

  co_return std::error_code{};
}

nx::awaitable_expected<sdp::SdpSession>
RtspClient::describe()
{
  if (m_session.state() < RtspSessionState::kConnected) {
    co_return std::unexpected(make_error_code(RtspErrc::not_connected));
  }

  // OPTIONS 먼저 전송 (서버 지원 메서드 확인)
  {
    RtspRequest options_req;
    options_req.method = RtspMethod::kOptions;
    options_req.uri = m_rtsp_url;
    options_req.cseq = m_session.next_cseq();
    options_req.headers["User-Agent"] = "HiVe2 RTSP Client";

    auto options_result = co_await send_request_with_auth(std::move(options_req));
    if (!options_result) {
      spdlog::warn("OPTIONS 실패 (무시하고 계속): {}", options_result.error().message());
    }
  }

  // DESCRIBE 요청
  RtspRequest request;
  request.method = RtspMethod::kDescribe;
  request.uri = m_rtsp_url;
  request.cseq = m_session.next_cseq();
  request.headers["Accept"] = "application/sdp";
  request.headers["User-Agent"] = "HiVe2 RTSP Client";

  auto response = co_await send_request_with_auth(std::move(request));
  if (!response) {
    co_return std::unexpected(response.error());
  }

  if (!response->is_success()) {
    co_return std::unexpected(
      make_error_code(rtsp_status_to_errc(response->status_code)));
  }

  // SDP 파싱
  auto sdp_result = sdp::SdpParser::parse(response->body);
  if (!sdp_result) {
    spdlog::error("SDP 파싱 실패: {}", sdp_result.error().message());
    co_return std::unexpected(make_error_code(RtspErrc::sdp_parse_error));
  }

  m_session.set_sdp(*sdp_result);
  m_session.set_state(RtspSessionState::kDescribed);

  spdlog::info("DESCRIBE 성공: {} 미디어 트랙", sdp_result->media_descriptions().size());

  co_return *sdp_result;
}

nx::awaitable<std::error_code>
RtspClient::setup(RtspTransport preferred_transport)
{
  if (m_session.state() < RtspSessionState::kDescribed) {
    co_return make_error_code(RtspErrc::invalid_state);
  }

  const auto& sdp = m_session.sdp();
  if (!sdp) {
    co_return make_error_code(RtspErrc::setup_required);
  }

  uint8_t interleaved_channel = 0;
  uint32_t track_id = 0;

  for (const auto& media : sdp->media_descriptions()) {
    RtspRequest request;
    request.method = RtspMethod::kSetup;
    request.uri = build_control_url(media.control_url);
    request.cseq = m_session.next_cseq();
    request.headers["User-Agent"] = "HiVe2 RTSP Client";

    // 이전 SETUP에서 받은 Session ID가 있으면 포함
    if (!m_session.session_id().empty()) {
      request.headers["Session"] = m_session.session_id();
    }

    // Transport 헤더 설정
    if (preferred_transport == RtspTransport::kRtpTcp) {
      request.headers["Transport"]
        = "RTP/AVP/TCP;unicast;interleaved=" + std::to_string(interleaved_channel) + "-"
          + std::to_string(interleaved_channel + 1);
    }
    else {
      // UDP 모드
      request.headers["Transport"] = "RTP/AVP;unicast;client_port=50000-50001";
    }

    auto response = co_await send_request_with_auth(std::move(request));
    if (!response) {
      co_return response.error();
    }

    if (!response->is_success()) {
      co_return make_error_code(rtsp_status_to_errc(response->status_code));
    }

    // Session 헤더 파싱
    auto session_header = response->find_header("Session");
    if (!session_header.empty()) {
      parse_session_header(session_header);
    }

    // Transport 헤더 파싱
    auto transport_header = response->find_header("Transport");
    if (!transport_header.empty()) {
      auto transport_result = RtspParser::parse_transport(transport_header);
      if (transport_result) {
        m_session.add_transport(*transport_result);
      }
    }

    // 트랙 정보 생성
    TrackInfo track;
    track.track_id = track_id;
    track.media_type
      = (media.type == sdp::SdpMediaType::kVideo)   ? media::MediaType::kVideo
        : (media.type == sdp::SdpMediaType::kAudio) ? media::MediaType::kAudio
                                                    : media::MediaType::kUnknown;
    track.rtp_channel = interleaved_channel;
    track.rtcp_channel = interleaved_channel + 1;

    // RTP 수신기 생성
    if (preferred_transport == RtspTransport::kRtpTcp) {
      track.receiver = std::make_unique<rtp::RtpReceiver>(
        m_ioc,
        rtp::RtpReceiver::TransportMode::kTcpInterleaved);
    }
    else {
      track.receiver = std::make_unique<rtp::RtpReceiver>(
        m_ioc,
        rtp::RtpReceiver::TransportMode::kUdp);
    }

    // 비디오 디패킷타이저 설정
    if (media.type == sdp::SdpMediaType::kVideo) {
      // rtpmap에서 코덱 확인 (예: "H264/90000" → "H264")
      auto video_codec = media::VideoCodec::kUnknown;
      uint8_t video_pt = 0;
      for (const auto& [pt, codec_str] : media.rtpmap) {
        auto slash_pos = codec_str.find('/');
        auto codec_name = (slash_pos != std::string::npos)
                            ? std::string_view(codec_str).substr(0, slash_pos)
                            : std::string_view(codec_str);
        video_codec = media::video_codec_from_string(codec_name);
        if (video_codec != media::VideoCodec::kUnknown) {
          video_pt = static_cast<uint8_t>(pt);
          break;
        }
      }

      // 예상 RTP Payload Type 설정 (코덱 변경 시 PT 불일치 감지용)
      if (video_pt != 0) {
        track.receiver->set_expected_payload_type(video_pt);
      }

      switch (video_codec) {
        case media::VideoCodec::kH264:
          track.receiver->set_depacketizer(std::make_unique<rtp::RtpH264Depacketizer>());
          break;
        case media::VideoCodec::kH265:
          track.receiver->set_depacketizer(std::make_unique<rtp::RtpH265Depacketizer>());
          break;
        case media::VideoCodec::kMjpeg:
          track.receiver->set_depacketizer(std::make_unique<rtp::RtpJpegDepacketizer>());
          break;
        default: spdlog::warn("지원하지 않는 비디오 코덱, 디패킷타이저 미설정"); break;
      }

      // 디패킷타이저에 코덱 불일치 감지 콜백 연결
      if (auto* depk = track.receiver->get_depacketizer(); depk) {
        auto tid_capture = track_id;
        depk->set_codec_mismatch_callback([this, tid_capture](uint32_t error_count) {
          if (m_codec_mismatch_callback) {
            m_codec_mismatch_callback(tid_capture, error_count);
          }
        });
      }

      // RTP PT 불일치 콜백 연결 (동일 코덱 변경 감지 체인 사용)
      {
        auto tid_capture = track_id;
        track.receiver->set_payload_type_mismatch_callback(
          [this, tid_capture](uint8_t /*expected_pt*/, uint8_t /*actual_pt*/) {
            if (m_codec_mismatch_callback) {
              m_codec_mismatch_callback(tid_capture, 3);
            }
          });
      }
    }

    // 프레임 콜백 설정
    auto tid = track_id;
    auto mtype = track.media_type;
    track.receiver->set_frame_callback(
      [this,
       tid,
       mtype](nx::media::SharedFrameData frame, uint64_t timestamp, bool keyframe) {
        if (m_media_callback) {
          m_media_callback(tid, mtype, std::move(frame), timestamp, keyframe);
        }
      });

    m_tracks.push_back(std::move(track));

    interleaved_channel += 2;
    track_id++;

    spdlog::info(
      "SETUP 완료: track_id={}, type={}",
      track_id - 1,
      (mtype == media::MediaType::kVideo) ? "video" : "audio");
  }

  m_session.set_state(RtspSessionState::kReady);
  co_return std::error_code{};
}

nx::awaitable<std::error_code>
RtspClient::play(double start, double end)
{
  if (m_session.state() < RtspSessionState::kReady) {
    co_return make_error_code(RtspErrc::setup_required);
  }

  RtspRequest request;
  request.method = RtspMethod::kPlay;
  request.uri = m_rtsp_url;
  request.cseq = m_session.next_cseq();
  request.headers["User-Agent"] = "HiVe2 RTSP Client";

  if (!m_session.session_id().empty()) {
    request.headers["Session"] = m_session.session_id();
  }

  // Range 설정
  if (start >= 0.0) {
    std::string range = "npt=" + std::to_string(start) + "-";
    if (end >= 0.0) {
      range += std::to_string(end);
    }
    request.headers["Range"] = range;
  }

  auto response = co_await send_request_with_auth(std::move(request));
  if (!response) {
    co_return response.error();
  }

  if (!response->is_success()) {
    co_return make_error_code(rtsp_status_to_errc(response->status_code));
  }

  m_session.set_state(RtspSessionState::kPlaying);
  m_receiving = true;

  // TCP Interleaved 수신 시작
  bool has_tcp_track = false;
  for (const auto& track : m_tracks) {
    if (track.receiver) {
      has_tcp_track = true;
      break;
    }
  }

  if (has_tcp_track) {
    boost::asio::co_spawn(m_ioc, receive_interleaved_loop(), boost::asio::detached);
  }

  spdlog::info("PLAY 시작");
  co_return std::error_code{};
}

nx::awaitable<std::error_code>
RtspClient::pause()
{
  if (m_session.state() != RtspSessionState::kPlaying) {
    co_return make_error_code(RtspErrc::invalid_state);
  }

  RtspRequest request;
  request.method = RtspMethod::kPause;
  request.uri = m_rtsp_url;
  request.cseq = m_session.next_cseq();
  request.headers["User-Agent"] = "HiVe2 RTSP Client";

  if (!m_session.session_id().empty()) {
    request.headers["Session"] = m_session.session_id();
  }

  auto response = co_await send_request_with_auth(std::move(request));
  if (!response) {
    co_return response.error();
  }

  if (!response->is_success()) {
    co_return make_error_code(rtsp_status_to_errc(response->status_code));
  }

  m_receiving = false;
  m_session.set_state(RtspSessionState::kPaused);
  spdlog::info("PAUSE");
  co_return std::error_code{};
}

nx::awaitable<std::error_code>
RtspClient::teardown()
{
  if (m_session.state() < RtspSessionState::kReady) {
    co_return make_error_code(RtspErrc::invalid_state);
  }

  bool was_receiving = m_receiving;
  m_receiving = false;

  // 수신 루프가 동작 중이면 소켓의 비동기 작업을 취소
  if (was_receiving && m_connection && m_connection->is_connected()) {
    boost::system::error_code cancel_ec;
    m_connection->socket().cancel(cancel_ec);
  }

  // 수신 루프가 완전히 종료될 때까지 대기
  while (m_receive_loop_active.load()) {
    co_await AsioSteadyTimer(m_ioc, nx::milliseconds(5))
      .async_wait(boost::asio::use_awaitable);
  }

  RtspRequest request;
  request.method = RtspMethod::kTeardown;
  request.uri = m_rtsp_url;
  request.cseq = m_session.next_cseq();
  request.headers["User-Agent"] = "HiVe2 RTSP Client";

  if (!m_session.session_id().empty()) {
    request.headers["Session"] = m_session.session_id();
  }

  auto response = co_await send_request_with_auth(std::move(request));
  if (!response) {
    // TEARDOWN 실패는 무시하고 정리 진행
    spdlog::warn("TEARDOWN 실패: {}", response.error().message());
  }

  // 트랙 정리
  for (auto& track : m_tracks) {
    if (track.receiver) {
      track.receiver->stop();
    }
  }
  m_tracks.clear();

  m_session.set_state(RtspSessionState::kConnected);
  spdlog::info("TEARDOWN 완료");
  co_return std::error_code{};
}

nx::awaitable<std::error_code>
RtspClient::close()
{
  m_receiving = false;

  // 수신 루프가 동작 중이면 소켓 취소 후 종료 대기
  if (m_receive_loop_active.load() && m_connection && m_connection->is_connected()) {
    boost::system::error_code cancel_ec;
    m_connection->socket().cancel(cancel_ec);
  }
  while (m_receive_loop_active.load()) {
    co_await AsioSteadyTimer(m_ioc, nx::milliseconds(5))
      .async_wait(boost::asio::use_awaitable);
  }

  // 트랙 정리
  for (auto& track : m_tracks) {
    if (track.receiver) {
      track.receiver->stop();
    }
  }
  m_tracks.clear();

  if (m_connection) {
    co_await m_connection->close();
  }

  m_session.reset();
  spdlog::info("RTSP 연결 종료");
  co_return std::error_code{};
}

void
RtspClient::set_media_callback(MediaFrameCallback callback)
{
  m_media_callback = std::move(callback);
}

void
RtspClient::set_disconnect_callback(DisconnectCallback callback)
{
  m_disconnect_callback = std::move(callback);
}

void
RtspClient::set_codec_mismatch_callback(CodecMismatchCallback callback)
{
  m_codec_mismatch_callback = std::move(callback);
}

void
RtspClient::set_auth_provider(std::unique_ptr<auth::AuthProvider> provider)
{
  m_auth_provider = std::move(provider);
  m_use_auto_auth = false;
}

void
RtspClient::set_credentials(const std::string& username, const std::string& password)
{
  m_credentials.username = username;
  m_credentials.password = password;
  m_use_auto_auth = true;
}

bool
RtspClient::is_connected() const noexcept
{
  return m_connection && m_connection->is_connected();
}

RtspSessionState
RtspClient::state() const noexcept
{
  return m_session.state();
}

size_t
RtspClient::track_count() const noexcept
{
  return m_tracks.size();
}

std::optional<rtp::RtpStatistics>
RtspClient::get_track_statistics(uint32_t track_id) const noexcept
{
  for (const auto& track : m_tracks) {
    if (track.track_id == track_id && track.receiver) {
      return track.receiver->get_statistics();
    }
  }
  return std::nullopt;
}

std::optional<media::MediaType>
RtspClient::get_track_media_type(uint32_t track_id) const noexcept
{
  for (const auto& track : m_tracks) {
    if (track.track_id == track_id) {
      return track.media_type;
    }
  }
  return std::nullopt;
}

nx::awaitable_expected<RtspResponse>
RtspClient::send_request(const RtspRequest& request)
{
  if (!m_connection || !m_connection->is_connected()) {
    co_return std::unexpected(make_error_code(RtspErrc::not_connected));
  }

  // 요청 직렬화 및 전송
  auto data = request.serialize();
  spdlog::debug("RTSP 요청 전송:\n{}", data);

  auto ec = co_await m_connection->send(data);
  if (ec) {
    co_return std::unexpected(ec);
  }

  // 응답 수신
  auto response = co_await m_connection->receive_response();
  if (!response) {
    co_return std::unexpected(response.error());
  }

  spdlog::debug("RTSP 응답: {} {}", response->status_code, response->reason_phrase);

  co_return *response;
}

nx::awaitable_expected<RtspResponse>
RtspClient::send_request_with_auth(RtspRequest request)
{
  // 인증 헤더 적용
  apply_auth(request);

  auto response = co_await send_request(request);
  if (!response) {
    co_return response;
  }

  // 401 Unauthorized -> 인증 재시도
  if (response->status_code == 401 && m_use_auto_auth) {
    auto www_auth = response->find_header("WWW-Authenticate");
    if (!www_auth.empty()) {
      // Challenge 파싱
      auto challenge = auth::AuthChallengeParser::parse(www_auth);
      if (challenge) {
        // 인증 제공자 생성
        if (challenge->scheme == auth::AuthScheme::kDigest) {
          m_auth_provider = auth::AuthProviderFactory::create_digest(m_credentials);
        }
        else {
          m_auth_provider = auth::AuthProviderFactory::create_basic(m_credentials);
        }

        m_auth_provider->process_challenge(*challenge);

        // 요청 재전송
        request.cseq = m_session.next_cseq();
        apply_auth(request);
        auto retry_response = co_await send_request(request);
        co_return retry_response;
      }
    }
  }

  co_return response;
}

void
RtspClient::apply_auth(RtspRequest& request)
{
  if (!m_auth_provider) {
    return;
  }

  auth::AuthContext context;
  context.method = rtsp_method_to_string(request.method);
  context.uri = request.uri;

  auto auth_header = m_auth_provider->generate_authorization_header(context);
  if (auth_header) {
    request.headers["Authorization"] = *auth_header;
  }
}

void
RtspClient::parse_session_header(const std::string& session_header)
{
  // Session: <session-id>[;timeout=<seconds>]
  auto semicolon = session_header.find(';');
  if (semicolon != std::string::npos) {
    m_session.set_session_id(session_header.substr(0, semicolon));

    auto timeout_pos = session_header.find("timeout=", semicolon);
    if (timeout_pos != std::string::npos) {
      auto timeout_str = session_header.substr(timeout_pos + 8);
      try {
        auto timeout = std::stoul(timeout_str);
        m_session.set_timeout(nx::seconds(timeout));
      }
      catch (...) {
        // 타임아웃 파싱 실패는 무시
      }
    }
  }
  else {
    m_session.set_session_id(session_header);
  }
}

std::string
RtspClient::build_control_url(const std::string& control) const
{
  if (control.empty()) {
    return m_rtsp_url;
  }

  // 절대 URL
  if (control.find("rtsp://") == 0) {
    return control;
  }

  // 상대 URL
  std::string base = m_rtsp_url;

  // 마지막 '/' 기준으로 조합
  if (!base.empty() && base.back() != '/') {
    base += '/';
  }

  return base + control;
}

RtspClient::ReceiveLoopStats
RtspClient::get_receive_loop_stats()
{
  const auto count = m_recv_count.exchange(0, std::memory_order_relaxed);
  const auto total_us = m_recv_total_wait_us.exchange(0, std::memory_order_relaxed);
  const auto max_us = m_recv_max_wait_us.exchange(0, std::memory_order_relaxed);

  ReceiveLoopStats stats;
  stats.recv_count = count;
  stats.max_recv_wait_us = static_cast<double>(max_us);
  stats.avg_recv_wait_us
    = (count > 0) ? static_cast<double>(total_us) / static_cast<double>(count) : 0.0;
  return stats;
}

nx::awaitable<void>
RtspClient::receive_interleaved_loop()
{
  m_receive_loop_active.store(true);
  spdlog::debug("TCP Interleaved 수신 루프 시작");

  while (m_receiving && m_connection && m_connection->is_connected()) {
    const auto wait_start = std::chrono::steady_clock::now();
    auto frame_result = co_await m_connection->receive_interleaved_frame();
    const auto wait_us = static_cast<uint64_t>(nx::duration_count<nx::microseconds>(
      std::chrono::steady_clock::now() - wait_start));

    // 대기 시간 통계 업데이트 (원자 연산)
    m_recv_total_wait_us.fetch_add(wait_us, std::memory_order_relaxed);
    m_recv_count.fetch_add(1, std::memory_order_relaxed);
    // max 업데이트 (CAS 루프)
    {
      auto cur = m_recv_max_wait_us.load(std::memory_order_relaxed);
      while (wait_us > cur
             && !m_recv_max_wait_us.compare_exchange_weak(
               cur,
               wait_us,
               std::memory_order_relaxed)) {}
    }

    if (!frame_result) {
      if (m_receiving) {
        spdlog::error("Interleaved 프레임 수신 실패: {}", frame_result.error().message());
      }
      break;
    }

    auto& frame = *frame_result;

    // 채널에 해당하는 트랙 찾기
    for (auto& track : m_tracks) {
      if (frame.channel == track.rtp_channel && track.receiver) {
        // RTP 데이터
        track.receiver->feed_data(frame.channel, frame.data);
        break;
      }
      // RTCP 채널은 현재 무시
    }
  }

  // 비정상 종료 시 disconnect 콜백 호출 (m_receiving이 true인 상태로 루프 탈출)
  if (m_receiving && m_disconnect_callback) {
    m_disconnect_callback();
  }

  m_receive_loop_active.store(false);
  spdlog::debug("TCP Interleaved 수신 루프 종료");
}

nx::awaitable<void>
RtspClient::keep_alive_loop()
{
  auto timeout = m_session.timeout();
  // Keep-alive 간격: 세션 타임아웃의 절반
  auto interval = std::chrono::duration_cast<nx::seconds>(timeout) / 2;
  if (interval.count() < 5) {
    interval = nx::seconds(5);
  }

  while (m_receiving) {
    co_await AsioSteadyTimer(m_ioc, interval).async_wait(boost::asio::use_awaitable);

    if (!m_receiving) {
      break;
    }

    RtspRequest request;
    request.method = RtspMethod::kGetParameter;
    request.uri = m_rtsp_url;
    request.cseq = m_session.next_cseq();
    request.headers["User-Agent"] = "HiVe2 RTSP Client";

    if (!m_session.session_id().empty()) {
      request.headers["Session"] = m_session.session_id();
    }

    auto response = co_await send_request(request);
    if (!response) {
      spdlog::warn("Keep-alive 실패: {}", response.error().message());
    }
  }
}

} // namespace nx::net
