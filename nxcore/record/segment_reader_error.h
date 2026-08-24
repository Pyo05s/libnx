// 파일: segment_reader_error.h
// 생성일: 2026-04-10
// 설명: SegmentReader 전용 에러 코드 정의 (세그먼트 파일 파싱 관련)

#pragma once

#include <string>
#include <system_error>

namespace nx {
namespace record {

/// 세그먼트 파일 파싱 에러 코드
enum class SegmentReaderErrc : int
{
  kSuccess = 0,

  // 파일 관련
  kSegmentFileNotFound = 20,
  kSegmentFileCorrupted = 21,
  kInvalidSegmentHeader = 22,
  kInvalidBlockHeader = 23,
  kInvalidEntryHeader = 24,
  kUnexpectedEof = 25,
  kEndOfSegment = 26, ///< 세그먼트 데이터 정상 종료 (footer 도달)

  // 읽기 관련
  kReadError = 30,
  kSeekFailed = 31,
  kFooterParseError = 32,
};

// ============================================================================
// std::error_category 구현
// ============================================================================

namespace detail {

class SegmentReaderErrorCategory : public std::error_category
{
public:
  const char* name() const noexcept override { return "segment_reader"; }

  std::string message(int ev) const override
  {
    switch (static_cast<SegmentReaderErrc>(ev)) {
      case SegmentReaderErrc::kSuccess: return "성공";
      case SegmentReaderErrc::kSegmentFileNotFound:
        return "세그먼트 파일을 찾을 수 없음";
      case SegmentReaderErrc::kSegmentFileCorrupted: return "세그먼트 파일 손상";
      case SegmentReaderErrc::kInvalidSegmentHeader:
        return "유효하지 않은 세그먼트 헤더";
      case SegmentReaderErrc::kInvalidBlockHeader: return "유효하지 않은 블록 헤더";
      case SegmentReaderErrc::kInvalidEntryHeader: return "유효하지 않은 엔트리 헤더";
      case SegmentReaderErrc::kUnexpectedEof: return "예상치 못한 파일 끝";
      case SegmentReaderErrc::kEndOfSegment: return "세그먼트 데이터 정상 종료";
      case SegmentReaderErrc::kReadError: return "읽기 오류";
      case SegmentReaderErrc::kSeekFailed: return "탐색 실패";
      case SegmentReaderErrc::kFooterParseError: return "푸터 파싱 오류";
      default: return "알 수 없는 SegmentReader 에러";
    }
  }
};

} // namespace detail

/// 에러 카테고리 싱글턴
inline const std::error_category&
segment_reader_error_category()
{
  static detail::SegmentReaderErrorCategory instance;
  return instance;
}

/// error_code 생성 헬퍼
inline std::error_code
make_error_code(SegmentReaderErrc e)
{
  return {static_cast<int>(e), segment_reader_error_category()};
}

} // namespace record
} // namespace nx

// std::error_code 변환 지원
template <>
struct std::is_error_code_enum<nx::record::SegmentReaderErrc> : std::true_type
{};
