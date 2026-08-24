// 파일: cipher.h
// 생성일: 2026-03-17
// 설명: AES-256-GCM 대칭 암호화/복호화 기능

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include "nxcore/util/type_util.h"

#include <expected>
#include <system_error>

namespace nx::crypto {

// AES-256-GCM 상수
inline constexpr size_t kAesKeyLength = 32;   // 256비트 키
inline constexpr size_t kAesNonceLength = 12; // 96비트 Nonce (GCM 권장)
inline constexpr size_t kAesTagLength = 16;   // 128비트 인증 태그

// 최소 암호문 크기: Nonce(12) + Tag(16) = 28 바이트 (빈 평문 기준)
inline constexpr size_t kMinCiphertextLength = kAesNonceLength + kAesTagLength;

class Cipher
{
public:
  NX_NON_INSTANTIABLE(Cipher);

  // AES-256-GCM 암호화
  // 입력: 평문 바이트, 256비트(32바이트) 키
  // 출력: [Nonce 12B | Ciphertext | AuthTag 16B]
  static nx::expected<Bytes> encrypt(BytesView plaintext, BytesView key);

  // AES-256-GCM 복호화
  // 입력: [Nonce 12B | Ciphertext | AuthTag 16B], 256비트(32바이트) 키
  // 출력: 평문 바이트
  static nx::expected<Bytes> decrypt(BytesView ciphertext, BytesView key);
};

} // namespace nx::crypto
