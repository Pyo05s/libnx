// 파일: record.h
// 생성일: 2025-11-24
// 설명: Recorder 주요 타입들을 정의 합니다. (nx::record 네임스페이스)

#pragma once

#include "../util/time_util.h"
#include "block_buffer_pool.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace nx {
namespace record {

#pragma pack(push, 1)

// ===================================================================
// 열거형 정의
// ===================================================================

// 포멧 버전
enum FormatVersion
{
  kV1 = 0x00000001,
  kV2 = 0x00000002, // IndexEntry flags에 entry_type/codec_type 인코딩

  kFormatVersion = kV2
};

// 블록 타입
enum class BlockType : uint8_t
{
  kVideo = 0x01,    // 영상 데이터
  kAudio = 0x02,    // 오디오 데이터
  kUserdata = 0x03, // 사용자 정의 데이터
};

// 엔트리 타입
enum class EntryType : uint8_t
{
  kVideo = 0x01,    // 영상 엔트리
  kAudio = 0x02,    // 오디오 엔트리
  kUserdata = 0x03, // 사용자 정의 데이터 엔트리
};

// 비디오 코덱 타입
enum class VideoCodecType : uint8_t
{
  kH264 = 0x01,  // H.264
  kH265 = 0x02,  // H.265/HEVC
  kMJPEG = 0x03, // Motion JPEG
};

// 비디오 프레임 타입
enum class VideoFrameType : uint8_t
{
  kIFrame = 0x01, // I-프레임 (키프레임)
  kPFrame = 0x02, // P-프레임
  kBFrame = 0x03, // B-프레임
};

// 오디오 코덱 타입
enum class AudioCodecType : uint8_t
{
  kAAC = 0x01,  // AAC
  kG711 = 0x02, // G.711
  kPCM = 0x03,  // PCM
};

// 데이터 동기화 레벨
enum class DataSyncLevel : uint8_t
{
  kNone = 0x00,   // 동기화 없음 (OS 버퍼 캐싱에만 의존)
  kNormal = 0x01, // C++ 스트림 플러시만 수행
  kFull = 0x02,   // C++ 스트림 플러시 + OS 레벨 동기화 (fsync/FlushFileBuffers)
};

// 쓰기 오류 처리 정책
enum class WriteErrorPolicy : uint8_t
{
  kDiscard = 0x00, // 즉시 버림 (기본값)
};

// 블록 오버플로우 정책
enum class OverflowPolicy : uint8_t
{
  kDropOldest = 0x00, // 오래된 블록 제거 (기본값)
  kDropNewest = 0x01, // 새 entry 무시
};

// 데이터 블록 플래그
enum class BlockFlags : uint32_t
{
  kNone = 0x00000000,
  kCompressed = 0x00000001,  // 블록 데이터가 압축됨
  kHasKeyFrame = 0x00000002, // 블록에 키프레임 포함

  // Storage Pressure 비상 녹화 플래그 (Phase 2/3)
  kProfileDowngraded = 0x00000004, // 낮은 프로파일로 전환되어 녹화된 블록
  kKeyframeOnly = 0x00000008,      // 키프레임 전용 모드로 녹화된 블록

  kBlockAttrMask = 0x0000FFFF,  // 블록 속성 마스크
  kRecordAttrMask = 0xFFFF0000, // 녹화 속성 마스크
};

// ===================================================================
// 세그먼트 파일 구조
// ===================================================================
// | 세그먼트 헤더 | 데이터 블록 1 | 데이터 블록 2 | ... | 세그먼트 푸터 |

// ===================================================================
// 세그먼트 헤더
// 세그먼트 헤더는 파일 생성 시에 가장 먼저 생성 되고, 파일이 닫힐 때까지 변경
// 되지 않도록 해야 합니다.
// ===================================================================

struct SegmentHeader
{
  static constexpr uint16_t kMagic = 0xFFF0;

  uint16_t magic;                 // 헤더 매직 넘버
  uint16_t header_size;           // 헤더 크기 (바이트 단위)
  uint32_t version;               // 세그먼트 포맷 버전
  int64_t channel_id;             // 채널 ID
  uint32_t extension_header_size; // 확장 헤더 크기 (바이트 단위)
};

// ===================================================================
// 데이터 블록 구조
// ===================================================================
// | 데이터 블록 헤더 | 데이터 블록 엔트리 1 | ... | 데이터 블록 엔트리 N | 0xFFFB |
//                  /          |         \
    // | 블록 엔트리 공통 헤더 | 블록 엔트리 전용 필드 | 블록 엔트리 페이로드 |

static constexpr uint16_t kBlockEndMagic = 0xFFFB;

// ===================================================================
// 데이터 블록 헤더
// ===================================================================

struct BlockHeader
{
  static constexpr uint16_t kMagic = 0xFFF1;

  uint16_t magic;           // 블록 시작 매직 넘버
  uint16_t header_size;     // 블록 헤더 크기 (바이트 단위)
  uint32_t flags;           // 블록 플래그 (압축 여부 등)
  uint32_t length;          // 블록 데이터 길이 (바이트 단위, magic number 포함)
  mstime_t start_timestamp; // 블록 시작 타임스탬프 (밀리초, Unix Epoch)
  mstime_t end_timestamp;   // 블록 종료 타임스탬프 (밀리초, Unix Epoch)
};

// ===================================================================
// 데이터 블록 엔트리 구조
// ===================================================================

// 기본 엔트리 구조
struct BlockEntry
{
  uint8_t type;         // 엔트리 타입 (영상, 오디오, 데이터)
  uint8_t archive_type; // 아카이브 타입 (압축 방식, 버전 등)
  uint16_t header_size; // 엔트리 헤더 전체 크기 (파생 엔트리 포함) - 직렬화 시 사용
  mstime_t timestamp;   // 엔트리 타임스탬프 (밀리초, Unix Epoch)
};

// 영상 데이터 블록 엔트리
struct VideoBlockEntry : public BlockEntry
{
  // 영상 전용 필드
  uint8_t codec_type;    // 코덱 타입 (H264, MJPEG 등)
  uint8_t frame_type;    // 프레임 타입 (I-프레임, P-프레임 등)
  uint8_t reserved1[2];  // 4byte 정렬용 예약 필드
  uint32_t payload_size; // 페이로드 크기 (바이트 단위)
};

// 오디오 데이터 블록 엔트리
struct AudioBlockEntry : public BlockEntry
{
  // 오디오 전용 필드
  uint8_t codec_type;    // 코덱 타입 (AAC, G711 등)
  uint8_t channels;      // 채널 수 (모노=1, 스테레오=2)
  uint8_t bit_depth;     // 비트 깊이 (예: 16, 24)
  uint32_t sample_rate;  // 샘플링 레이트 (예: 44100, 48000)
  uint32_t payload_size; // 페이로드 크기 (바이트 단위)
};

// 사용자 정의 데이터 블록 엔트리
struct UserBlockEntry : public BlockEntry
{
  // 사용자 데이터 전용 필드
  uint8_t data_type;     // 사용자 정의 데이터 타입
  uint8_t reserved[3];   // 4byte 정렬용 예약 필드
  uint32_t payload_size; // 페이로드 크기 (바이트 단위)
};

// ===================================================================
// 세그먼트 푸터 구조
// ===================================================================
// | 인덱스 엔트리 1 | 인덱스 엔트리 2 | ... | 인덱스 엔트리 N | 푸터 헤더 |

struct IndexEntry
{
  static constexpr uint16_t kMagic = 0xFFF2;

  uint16_t magic;     // 인덱스 엔트리 매직 넘버
  uint32_t flags;     // 인덱스 플래그 (비트 레이아웃은 IndexFlags 참조)
  mstime_t timestamp; // 데이터 블록 타임스탬프 (밀리초, Unix Epoch)
  uint64_t offset;    // 데이터 블록 오프셋 (세그먼트 파일 내)
};

// ===================================================================
// IndexEntry::flags 비트 레이아웃 (V2)
// ===================================================================
// [31..24] codec_type  — VideoCodecType / AudioCodecType / UserBlockEntry::data_type
// [23..16] entry_type  — EntryType (kVideo=0x01, kAudio=0x02, kUserdata=0x03)
// [15.. 0] block_flags — BlockFlags (kHasKeyFrame 등)
//
// V1 호환: V1 reader는 하위 16비트(BlockFlags)만 사용하므로 상위 비트 무시 가능

namespace index_flags {

// 비트 위치 상수
constexpr uint32_t kBlockFlagsMask = 0x0000FFFF;
constexpr uint32_t kEntryTypeShift = 16;
constexpr uint32_t kEntryTypeMask = 0x00FF0000;
constexpr uint32_t kCodecTypeShift = 24;
constexpr uint32_t kCodecTypeMask = 0xFF000000;

/// BlockFlags + EntryType + CodecType → flags 합성
constexpr uint32_t
encode(uint32_t block_flags, EntryType entry_type, uint8_t codec_type)
{
  return (block_flags & kBlockFlagsMask) |
         (static_cast<uint32_t>(entry_type) << kEntryTypeShift) |
         (static_cast<uint32_t>(codec_type) << kCodecTypeShift);
}

/// flags에서 BlockFlags 추출 (V1/V2 공통)
constexpr uint32_t
block_flags(uint32_t flags)
{
  return flags & kBlockFlagsMask;
}

/// flags에서 EntryType 추출 (V1이면 0 반환)
constexpr EntryType
entry_type(uint32_t flags)
{
  return static_cast<EntryType>((flags & kEntryTypeMask) >> kEntryTypeShift);
}

/// flags에서 codec_type 추출 (V1이면 0 반환)
constexpr uint8_t
codec_type(uint32_t flags)
{
  return static_cast<uint8_t>((flags & kCodecTypeMask) >> kCodecTypeShift);
}

/// V2 확장 정보가 인코딩되어 있는지 확인
constexpr bool
has_extended_info(uint32_t flags)
{
  return (flags & (kEntryTypeMask | kCodecTypeMask)) != 0;
}

} // namespace index_flags

// ===================================================================
// 세그먼트 푸터 헤더
// ===================================================================

struct FooterHeader
{
  static constexpr uint16_t kMagicStart = 0xFFF3;
  static constexpr uint16_t kMagicEnd = 0xFFFE;

  uint16_t magic;       // 푸터 시작 매직 넘버
  uint16_t header_size; // 푸터 헤더 크기 (바이트 단위), sizeof(FooterHeader)
  uint32_t index_count; // 인덱스 엔트리 개수
  uint32_t index_size;  // 인덱스 전체 크기 (바이트 단위), 인덱스 시작 부터 푸터
                        // 헤더 전 까지
  uint8_t reserved[2];  // 4byte 정렬용 예약 필드
  uint16_t magic_end;   // 푸터 종료 매직 넘버 (0xFFF0)
};

// ===================================================================
// 인덱스 파일 구조
// ===================================================================
// | 인덱스 파일 헤더 | 인덱스 엔트리 1 | 인덱스 엔트리 2 | ... | 인덱스 엔트리 N |

struct IndexFileHeader
{
  static constexpr uint16_t kMagic = 0xFFFA;

  uint16_t magic;       // 헤더 매직 넘버
  uint16_t header_size; // 헤더 크기 (바이트 단위), sizeof(IndexFileHeader)
  uint32_t version;     // 인덱스 파일 포맷 버전
  int64_t channel_id;   // 채널 ID
};

#pragma pack(pop)

struct BlockEntryBuffer
{
  // 이제 entry 는 shared_ptr로 파생 엔트리 객체를 가리킵니다.
  std::shared_ptr<BlockEntry> entry; // 블록 엔트리 헤더 (파생형 가능)
  std::shared_ptr<std::vector<uint8_t>>
    payload; // 블록 엔트리 페이로드 (shared_ptr 기반 zero-copy)

  // Storage Pressure 플래그 (BlockFlags OR 조합 — BlockBuilder가
  // BlockHeader::flags에 반영)
  uint32_t pressure_flags = 0;
};

struct DataBlock
{
  BlockHeader const* header{nullptr};     // serialized 위의 header 위치, 직접 해제 금지
  std::vector<BlockEntry const*> entries; // serialized 위의 entry 위치, 직접 해제 금지
  uint16_t const* end_magic{nullptr}; // serialized 위의 end magic 위치, 직접 해제 금지

  // 블록 전체가 직렬화된 버퍼, 내부 연속 메모리 소유 (풀 기반 자동 반환)
  PooledBuffer serialized;
};

} // namespace record
} // namespace nx