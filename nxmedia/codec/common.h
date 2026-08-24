// 파일: common.h
// 생성일: 2026-04-01
// 설명: 코덱 공통 유틸리티 - RBSP 에뮬레이션 방지 바이트 제거 등

#pragma once

#include <cstdint>
#include <span>
#include <vector>

namespace nx::media::codec {

/// RBSP 에뮬레이션 방지 바이트(0x03) 제거
/// NAL unit 내 00 00 03 시퀀스에서 0x03을 제거하여 RBSP 복원
/// @param data NAL unit 바이트 (NAL 헤더 이후 부분)
/// @return 에뮬레이션 방지 바이트가 제거된 RBSP 데이터
std::vector<uint8_t> remove_emulation_prevention(std::span<const uint8_t> data);

} // namespace nx::media::codec
