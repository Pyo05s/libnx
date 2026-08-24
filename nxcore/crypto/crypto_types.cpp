// 파일: crypto_types.cpp
// 생성일: 2026-02-10
// 설명: 암호화 모듈 공통 타입 유틸리티 구현

#include "nxcore/crypto/crypto_types.h"

#include <format>
#include <stdexcept>

namespace nx::crypto {

std::string
bytes_to_hex(BytesView data)
{
  if (data.empty()) {
    return "";
  }

  std::string result;
  result.reserve(data.size() * 2);

  for (uint8_t byte : data) {
    result += std::format("{:02x}", byte);
  }

  return result;
}

Bytes
hex_to_bytes(std::string_view hex)
{
  if (hex.length() % 2 != 0) {
    throw std::invalid_argument("Hex string must have even length");
  }

  Bytes result;
  result.reserve(hex.length() / 2);

  for (size_t i = 0; i < hex.length(); i += 2) {
    std::string byte_string(hex.substr(i, 2));
    uint8_t byte = static_cast<uint8_t>(std::stoi(byte_string, nullptr, 16));
    result.push_back(byte);
  }

  return result;
}

} // namespace nx::crypto
