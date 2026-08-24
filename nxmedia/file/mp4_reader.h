// 파일: mp4_reader.h
// 생성일: 2026-02-06
// 설명: FFmpeg를 이용한 MP4 파일 리더

#pragma once

#include <nxcore/media/media_type.h>

#include <filesystem>
#include <memory>
#include <string>
#include <system_error>
#include <vector>
#include <expected>

namespace nx {
namespace media {

class Mp4Reader
{
public:
  // 미디어 스트림 정보
  struct StreamInfo
  {
    int32_t index = -1; // 스트림 인덱스
    MediaType type = MediaType::kUnknown;
    std::string codec_name; // 코덱 이름 (h264, aac 등)
    int32_t bitrate = 0;

    // 비디오 전용 필드
    int32_t width = 0;
    int32_t height = 0;
    double fps = 0.0;

    // 오디오 전용 필드
    int32_t sample_rate = 0;
    int32_t channels = 0;

    // 코덱 구성 데이터 (H.264 avcC, H.265 hvcC 등)
    std::vector<uint8_t> codec_config;
  };

  // 전체 미디어 정보
  struct MediaInfo
  {
    int64_t duration_ms = 0;         // 전체 재생 시간 (밀리초)
    int32_t bitrate = 0;             // 전체 비트레이트
    std::vector<StreamInfo> streams; // 스트림 정보 목록
  };

  Mp4Reader();
  ~Mp4Reader();

  // 복사 금지, 이동 허용
  Mp4Reader(const Mp4Reader&) = delete;
  Mp4Reader& operator=(const Mp4Reader&) = delete;
  Mp4Reader(Mp4Reader&&) noexcept;
  Mp4Reader& operator=(Mp4Reader&&) noexcept;

  // 파일 열기 (자동으로 미디어 정보 파싱)
  std::error_code open(const std::filesystem::path& path);

  // 미디어 정보 조회 (open 성공 후 사용 가능)
  const MediaInfo& get_media_info() const noexcept;

  // 다음 프레임 읽기 (모든 스트림에서 순차적으로)
  // EOF 도달 시 kEndOfFile 오류 반환
  nx::expected<Frame> read_frame();

  // 특정 시간으로 이동 (키프레임 기준)
  // timestamp_ms: 목표 시간 (밀리초)
  // 실제로는 가장 가까운 이전 키프레임으로 이동
  std::error_code seek(mstime_t timestamp_ms);

  // 파일이 열려 있는지 확인
  bool is_open() const noexcept;

  // 명시적 닫기 (소멸자에서도 자동 호출)
  void close() noexcept;

private:
  struct Impl;
  std::unique_ptr<Impl> m_impl;
};

} // namespace media
} // namespace nx
