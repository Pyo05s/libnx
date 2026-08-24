// 파일: crypto_types.h
// 생성일: 2026-02-10
// 설명: 암호화 모듈의 공통 타입 및 유틸리티 함수 정의

#pragma once

#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace nx::crypto {

// 바이트 배열 타입
using Bytes = std::vector<uint8_t>;
using BytesView = std::span<const uint8_t>;

// 16진수 문자열 변환
std::string bytes_to_hex(BytesView data);
Bytes hex_to_bytes(std::string_view hex);

} // namespace nx::crypto
