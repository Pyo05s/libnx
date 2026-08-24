// 파일: mp4_reader.cpp
// 생성일: 2026-02-06
// 설명: Mp4Reader 구현

#include "mp4_reader.h"
#include "ffmpeg_error.h"

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavcodec/bsf.h>
#include <libavformat/avformat.h>
#include <libavutil/error.h>

}

#include <unordered_map>

namespace nx {
namespace media {

namespace {

// AVMediaType을 MediaType으로 변환
MediaType
convert_media_type(AVMediaType av_type)
{
  switch (av_type) {
  case AVMEDIA_TYPE_VIDEO:
    return MediaType::kVideo;
  case AVMEDIA_TYPE_AUDIO:
    return MediaType::kAudio;
  case AVMEDIA_TYPE_DATA:
  case AVMEDIA_TYPE_SUBTITLE:
    return MediaType::kMetadata;
  default:
    return MediaType::kUnknown;
  }
}

// AVRational을 double로 변환
double
av_rational_to_double(AVRational rational)
{
  if (rational.den == 0) {
    return 0.0;
  }
  return static_cast<double>(rational.num) / static_cast<double>(rational.den);
}

std::string
av_error_to_string(int errnum)
{
  char buf[128];
  av_strerror(errnum, buf, sizeof(buf));
  return std::string(buf);
}

const char*
annex_b_filter_name(AVCodecID codec_id)
{
  switch (codec_id) {
  case AV_CODEC_ID_H264:
    return "h264_mp4toannexb";
  case AV_CODEC_ID_HEVC:
    return "hevc_mp4toannexb";
  default:
    return nullptr;
  }
}

} // anonymous namespace

// pImpl 구조체 정의
struct Mp4Reader::Impl
{
  AVFormatContext* format_ctx = nullptr;
  AVPacket* packet = nullptr;
  MediaInfo media_info;
  std::unordered_map<int32_t, AVBSFContext*> bitstream_filters;
  bool is_open = false;

  Impl() { packet = av_packet_alloc(); }

  ~Impl()
  {
    close();
    if (packet) {
      av_packet_free(&packet);
    }
  }

  void close()
  {
    for (auto& [_, bsf_ctx] : bitstream_filters) {
      av_bsf_free(&bsf_ctx);
    }
    bitstream_filters.clear();

    if (format_ctx) {
      avformat_close_input(&format_ctx);
      format_ctx = nullptr;
    }
    is_open = false;
    media_info = MediaInfo{};
  }

  std::error_code parse_media_info()
  {
    if (!format_ctx) {
      return make_error_code(FfmpegError::kInvalidState);
    }

    // 스트림 정보 수집
    int ret = avformat_find_stream_info(format_ctx, nullptr);
    if (ret < 0) {
      return from_av_error(ret);
    }

    // 전체 미디어 정보 설정
    if (format_ctx->duration != AV_NOPTS_VALUE) {
      media_info.duration_ms =
        format_ctx->duration / 1000; // microseconds to milliseconds
    }

    if (format_ctx->bit_rate > 0) {
      media_info.bitrate = static_cast<int32_t>(format_ctx->bit_rate);
    }

    // 각 스트림 정보 파싱
    for (unsigned int i = 0; i < format_ctx->nb_streams; i++) {
      AVStream* stream = format_ctx->streams[i];
      AVCodecParameters* codecpar = stream->codecpar;

      StreamInfo stream_info;
      stream_info.index = static_cast<int32_t>(i);
      stream_info.type = convert_media_type(codecpar->codec_type);

      // 코덱 정보
      const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
      if (codec) {
        stream_info.codec_name = codec->name;
      }

      stream_info.bitrate = static_cast<int32_t>(codecpar->bit_rate);

      // 비디오 스트림 정보
      if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
        stream_info.width = codecpar->width;
        stream_info.height = codecpar->height;

        // FPS 계산
        if (stream->avg_frame_rate.den > 0) {
          stream_info.fps = av_rational_to_double(stream->avg_frame_rate);
        }
        else if (stream->r_frame_rate.den > 0) {
          stream_info.fps = av_rational_to_double(stream->r_frame_rate);
        }

        if (const char* bsf_name = annex_b_filter_name(codecpar->codec_id)) {
          const AVBitStreamFilter* bsf = av_bsf_get_by_name(bsf_name);
          if (!bsf) {
            return make_error_code(FfmpegError::kInvalidFormat);
          }

          AVBSFContext* bsf_ctx = nullptr;
          ret = av_bsf_alloc(bsf, &bsf_ctx);
          if (ret < 0) {
            return from_av_error(ret);
          }

          ret = avcodec_parameters_copy(bsf_ctx->par_in, codecpar);
          if (ret < 0) {
            av_bsf_free(&bsf_ctx);
            return from_av_error(ret);
          }

          bsf_ctx->time_base_in = stream->time_base;
          ret = av_bsf_init(bsf_ctx);
          if (ret < 0) {
            av_bsf_free(&bsf_ctx);
            return from_av_error(ret);
          }

          bitstream_filters[stream_info.index] = bsf_ctx;
        }
      }
      // 오디오 스트림 정보
      else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO) {
        stream_info.sample_rate = codecpar->sample_rate;
        stream_info.channels = codecpar->ch_layout.nb_channels;
      }

      // 코덱 구성 데이터 복사 (extradata: avcC, hvcC 등)
      if (codecpar->extradata && codecpar->extradata_size > 0) {
        stream_info.codec_config.assign(
          codecpar->extradata, codecpar->extradata + codecpar->extradata_size);
      }

      media_info.streams.push_back(stream_info);
    }

    return {};
  }

  Frame convert_packet_to_frame(const AVPacket* pkt)
  {
    Frame frame;

    if (!pkt || pkt->stream_index < 0 ||
        pkt->stream_index >= static_cast<int>(format_ctx->nb_streams)) {
      return frame;
    }

    AVStream* stream = format_ctx->streams[pkt->stream_index];
    AVPacket* filtered_packet = nullptr;
    const AVPacket* data_packet = pkt;

    auto bsf_it = bitstream_filters.find(pkt->stream_index);
    if (bsf_it != bitstream_filters.end()) {
      AVBSFContext* bsf_ctx = bsf_it->second;
      AVPacket* input_packet = av_packet_alloc();
      filtered_packet = av_packet_alloc();
      if (!input_packet || !filtered_packet) {
        if (input_packet) {
          av_packet_free(&input_packet);
        }
        if (filtered_packet) {
          av_packet_free(&filtered_packet);
        }
        return frame;
      }

      int ret = av_packet_ref(input_packet, pkt);
      if (ret >= 0) {
        ret = av_bsf_send_packet(bsf_ctx, input_packet);
      }
      av_packet_free(&input_packet);

      if (ret >= 0) {
        ret = av_bsf_receive_packet(bsf_ctx, filtered_packet);
      }

      if (ret >= 0) {
        data_packet = filtered_packet;
      }
      else {
        av_packet_free(&filtered_packet);
        return frame;
      }
    }

    // 기본 정보 설정
    frame.type = convert_media_type(stream->codecpar->codec_type);
    frame.stream_index = pkt->stream_index;
    frame.is_keyframe = (data_packet->flags & AV_PKT_FLAG_KEY) != 0;
    frame.encoded = true;

    // 타임스탬프 변환 (time_base 기준 -> 밀리초)
    if (data_packet->pts != AV_NOPTS_VALUE) {
      int64_t ts = av_rescale_q(data_packet->pts, stream->time_base, AVRational{1, 1000});
      frame.timestamp = std::max(static_cast<int64_t>(0), ts);
    }
    else if (data_packet->dts != AV_NOPTS_VALUE) {
      int64_t ts = av_rescale_q(data_packet->dts, stream->time_base, AVRational{1, 1000});
      frame.timestamp = std::max(static_cast<int64_t>(0), ts);
    }

    // 지속 시간 변환
    if (data_packet->duration > 0) {
      frame.duration =
        av_rescale_q(data_packet->duration, stream->time_base, AVRational{1, 1000});
    }

    // 데이터 복사
    if (data_packet->data && data_packet->size > 0) {
      frame.data.assign(data_packet->data, data_packet->data + data_packet->size);
    }

    if (filtered_packet) {
      av_packet_free(&filtered_packet);
    }

    return frame;
  }
};

// Mp4Reader 구현
Mp4Reader::Mp4Reader()
    : m_impl(std::make_unique<Impl>())
{
}

Mp4Reader::~Mp4Reader() = default;

Mp4Reader::Mp4Reader(Mp4Reader&&) noexcept = default;
Mp4Reader& Mp4Reader::operator=(Mp4Reader&&) noexcept = default;

std::error_code
Mp4Reader::open(const std::filesystem::path& path)
{
  // 이미 열려 있으면 닫기
  if (m_impl->is_open) {
    close();
  }

  // UTF-8 경로 변환
  std::string path_str = reinterpret_cast<const char*>(path.u8string().c_str());

  // 파일 열기
  int ret = avformat_open_input(&m_impl->format_ctx, path_str.c_str(), nullptr, nullptr);
  if (ret < 0) {
    return from_av_error(ret);
  }

  // 미디어 정보 파싱
  auto ec = m_impl->parse_media_info();
  if (ec) {
    m_impl->close();
    return ec;
  }

  m_impl->is_open = true;

  return {};
}

const Mp4Reader::MediaInfo&
Mp4Reader::get_media_info() const noexcept
{
  return m_impl->media_info;
}

nx::expected<Frame>
Mp4Reader::read_frame()
{
  if (!m_impl->is_open || !m_impl->format_ctx) {
    return std::unexpected(make_error_code(FfmpegError::kInvalidState));
  }

  // 패킷 초기화
  av_packet_unref(m_impl->packet);

  // 패킷 읽기
  int ret = av_read_frame(m_impl->format_ctx, m_impl->packet);
  if (ret < 0) {
    if (ret == AVERROR_EOF) {
      return std::unexpected(make_error_code(FfmpegError::kEndOfFile));
    }
    return std::unexpected(from_av_error(ret));
  }

  // AVPacket을 Frame으로 변환
  Frame frame = m_impl->convert_packet_to_frame(m_impl->packet);

  return frame;
}

std::error_code
Mp4Reader::seek(mstime_t timestamp_ms)
{
  if (!m_impl->is_open || !m_impl->format_ctx) {
    return make_error_code(FfmpegError::kInvalidState);
  }

  // 밀리초를 AV_TIME_BASE 단위로 변환
  int64_t timestamp = timestamp_ms * 1000; // microseconds

  // AVSEEK_FLAG_BACKWARD: 가장 가까운 이전 키프레임으로 이동
  int ret = av_seek_frame(m_impl->format_ctx, -1, timestamp, AVSEEK_FLAG_BACKWARD);
  if (ret < 0) {
    return make_error_code(FfmpegError::kSeekFailed);
  }

  // 디코더 버퍼 플러시
  avformat_flush(m_impl->format_ctx);
  for (auto& [_, bsf_ctx] : m_impl->bitstream_filters) {
    av_bsf_flush(bsf_ctx);
  }

  return {};
}

bool
Mp4Reader::is_open() const noexcept
{
  return m_impl->is_open;
}

void
Mp4Reader::close() noexcept
{
  m_impl->close();
}

} // namespace media
} // namespace nx
