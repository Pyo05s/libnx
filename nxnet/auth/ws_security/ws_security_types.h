// 파일: ws_security_types.h
// 생성일: 2026-02-10
// 설명: WS-Security 전용 타입 정의

#pragma once

#include "nxcore/crypto/crypto_types.h"
#include <string>
#include <vector>

namespace nx::net::auth::ws_security {

// PasswordDigest 해시 알고리즘
enum class HashAlgorithm
{
  kSha1,   // SHA-1 (OASIS 표준, ONVIF 기본값)
  kSha256, // SHA-256 (최근 보안 요구사항)
  kSha512  // SHA-512 (고급 보안)
};

// UsernameToken 구조체
struct UsernameToken
{
  std::string username;
  std::string password;          // 원본 비밀번호
  bool is_digest;                // true: PasswordDigest, false: PasswordText
  HashAlgorithm algorithm;       // 해시 알고리즘 (Digest 모드일 때만 사용)
  nx::crypto::Bytes nonce_bytes; // 바이너리 nonce (16바이트)
  std::string nonce_base64;      // Base64 인코딩된 nonce
  std::string created;           // ISO 8601 UTC 타임스탬프
  std::string password_digest;   // Base64(HASH(nonce + created + password))
};

} // namespace nx::net::auth::ws_security
