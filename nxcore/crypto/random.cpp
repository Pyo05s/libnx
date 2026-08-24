// 파일: random.cpp
// 생성일: 2026-02-10
// 설명: 암호학적으로 안전한 난수 생성 구현 (OpenSSL 기반)

#include "nxcore/crypto/random.h"

#include <openssl/rand.h>

#include <format>

namespace nx::crypto {

namespace {

constexpr char kAlphanumeric[] = "0123456789"
                                 "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                 "abcdefghijklmnopqrstuvwxyz";

} // namespace

Bytes
Random::generate_bytes(size_t length)
{
  Bytes result(length);
  RAND_bytes(result.data(), static_cast<int>(length));
  return result;
}

std::string
Random::generate_uuid()
{
  // UUID v4 형식: xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx
  // 4: 버전 (v4)
  // y: variant (8, 9, a, b 중 하나)
  Bytes random_bytes = generate_bytes(16);

  // 버전 비트 설정 (v4)
  random_bytes[6] = (random_bytes[6] & 0x0F) | 0x40;

  // Variant 비트 설정 (RFC 4122)
  random_bytes[8] = (random_bytes[8] & 0x3F) | 0x80;

  return std::format(
    "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-"
    "{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
    random_bytes[0],
    random_bytes[1],
    random_bytes[2],
    random_bytes[3],
    random_bytes[4],
    random_bytes[5],
    random_bytes[6],
    random_bytes[7],
    random_bytes[8],
    random_bytes[9],
    random_bytes[10],
    random_bytes[11],
    random_bytes[12],
    random_bytes[13],
    random_bytes[14],
    random_bytes[15]);
}

std::string
Random::generate_alphanumeric(size_t length)
{
  Bytes random_bytes = generate_bytes(length);
  std::string result;
  result.reserve(length);

  constexpr size_t alphabet_size = sizeof(kAlphanumeric) - 1;
  for (uint8_t byte : random_bytes) {
    result += kAlphanumeric[byte % alphabet_size];
  }

  return result;
}

} // namespace nx::crypto
