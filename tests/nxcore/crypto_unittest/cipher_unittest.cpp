// 파일: cipher_unittest.cpp
// 생성일: 2026-03-17
// 설명: AES-256-GCM 암호화/복호화 단위 테스트

#include "nxcore/crypto/cipher.h"
#include "nxcore/crypto/base64.h"
#include "nxcore/crypto/crypto_error.h"
#include "nxcore/crypto/random.h"

#include <gtest/gtest.h>

#include <string_view>

using namespace nx::crypto;

namespace {

// 테스트용 32바이트 키 (고정)
const Bytes kTestKey
  = hex_to_bytes("0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef");

// 테스트용 잘못된 길이 키 (16바이트)
const Bytes kShortKey = hex_to_bytes("0123456789abcdef0123456789abcdef");

} // namespace

// ============================================================================
// 기본 암/복호화
// ============================================================================

TEST(CipherTest, EncryptDecryptRoundTrip)
{
  std::string plaintext = "admin1234";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 암호문 크기: Nonce(12) + plaintext(9) + Tag(16) = 37
  EXPECT_EQ(
    encrypted.value().size(),
    kAesNonceLength + plaintext.size() + kAesTagLength);

  auto decrypted = Cipher::decrypt(encrypted.value(), kTestKey);
  ASSERT_TRUE(decrypted.has_value());

  std::string result(decrypted.value().begin(), decrypted.value().end());
  EXPECT_EQ(result, plaintext);
}

TEST(CipherTest, EncryptDecryptEmptyPlaintext)
{
  Bytes empty;
  BytesView empty_view(empty);

  auto encrypted = Cipher::encrypt(empty_view, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 빈 평문: Nonce(12) + Tag(16) = 28
  EXPECT_EQ(encrypted.value().size(), kMinCiphertextLength);

  auto decrypted = Cipher::decrypt(encrypted.value(), kTestKey);
  ASSERT_TRUE(decrypted.has_value());
  EXPECT_TRUE(decrypted.value().empty());
}

TEST(CipherTest, EncryptDecryptLongPassword)
{
  std::string plaintext
    = "ThisIsAVeryLongPasswordThatExceeds40Characters!!@@##$$%%^^&&**";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  auto decrypted = Cipher::decrypt(encrypted.value(), kTestKey);
  ASSERT_TRUE(decrypted.has_value());

  std::string result(decrypted.value().begin(), decrypted.value().end());
  EXPECT_EQ(result, plaintext);
}

// ============================================================================
// Nonce 고유성
// ============================================================================

TEST(CipherTest, DifferentNonceEachEncryption)
{
  std::string plaintext = "test";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted1 = Cipher::encrypt(plain_bytes, kTestKey);
  auto encrypted2 = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted1.has_value());
  ASSERT_TRUE(encrypted2.has_value());

  // 동일한 평문이라도 매번 다른 암호문 생성 (Nonce가 다르므로)
  EXPECT_NE(encrypted1.value(), encrypted2.value());

  // 하지만 둘 다 같은 평문으로 복호화
  auto d1 = Cipher::decrypt(encrypted1.value(), kTestKey);
  auto d2 = Cipher::decrypt(encrypted2.value(), kTestKey);
  ASSERT_TRUE(d1.has_value());
  ASSERT_TRUE(d2.has_value());
  EXPECT_EQ(d1.value(), d2.value());
}

// ============================================================================
// 키 유효성 검증
// ============================================================================

TEST(CipherTest, EncryptInvalidKeyLength)
{
  std::string plaintext = "test";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto result = Cipher::encrypt(plain_bytes, kShortKey);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(CryptoError::kInvalidKeyLength));
}

TEST(CipherTest, DecryptInvalidKeyLength)
{
  // 유효한 암호문 생성
  std::string plaintext = "test";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 잘못된 키로 복호화 시도
  auto result = Cipher::decrypt(encrypted.value(), kShortKey);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(CryptoError::kInvalidKeyLength));
}

// ============================================================================
// 입력 유효성 검증
// ============================================================================

TEST(CipherTest, DecryptTooShortCiphertext)
{
  Bytes short_data = {1, 2, 3, 4, 5};
  auto result = Cipher::decrypt(short_data, kTestKey);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(CryptoError::kInvalidInput));
}

// ============================================================================
// 변조 감지 (인증 태그 검증)
// ============================================================================

TEST(CipherTest, TamperedCiphertextDetected)
{
  std::string plaintext = "secret_password";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 암호문 1바이트 변조
  auto tampered = encrypted.value();
  tampered[kAesNonceLength] ^= 0xFF;

  auto result = Cipher::decrypt(tampered, kTestKey);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), make_error_code(CryptoError::kDecryptFailure));
}

TEST(CipherTest, TamperedTagDetected)
{
  std::string plaintext = "password";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 인증 태그 변조
  auto tampered = encrypted.value();
  tampered.back() ^= 0xFF;

  auto result = Cipher::decrypt(tampered, kTestKey);
  ASSERT_FALSE(result.has_value());
}

TEST(CipherTest, WrongKeyDetected)
{
  std::string plaintext = "password";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  // 다른 키로 복호화 시도
  Bytes wrong_key = hex_to_bytes(
    "ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff");
  auto result = Cipher::decrypt(encrypted.value(), wrong_key);
  ASSERT_FALSE(result.has_value());
}

// ============================================================================
// Base64 연동 (DB 저장 시나리오)
// ============================================================================

TEST(CipherTest, EncryptBase64RoundTrip)
{
  std::string plaintext = "camera_pass_123";
  BytesView plain_bytes(
    reinterpret_cast<const uint8_t*>(plaintext.data()),
    plaintext.size());

  // 암호화 → Base64 인코딩 (DB 저장 형태)
  auto encrypted = Cipher::encrypt(plain_bytes, kTestKey);
  ASSERT_TRUE(encrypted.has_value());

  auto base64_str = Base64::encode(encrypted.value());
  EXPECT_FALSE(base64_str.empty());

  // 최소 Base64 길이 확인: ceil(28/3)*4 = 40
  EXPECT_GE(base64_str.size(), 40u);

  // Base64 디코딩 → 복호화
  auto decoded = Base64::decode(base64_str);
  ASSERT_TRUE(decoded.has_value());

  auto decrypted = Cipher::decrypt(decoded.value(), kTestKey);
  ASSERT_TRUE(decrypted.has_value());

  std::string result(decrypted.value().begin(), decrypted.value().end());
  EXPECT_EQ(result, plaintext);
}

// ============================================================================
// 최소 암호문 길이 상수 검증
// ============================================================================

TEST(CipherTest, MinCiphertextLengthConstant)
{
  EXPECT_EQ(kMinCiphertextLength, 28u);
  EXPECT_EQ(kAesKeyLength, 32u);
  EXPECT_EQ(kAesNonceLength, 12u);
  EXPECT_EQ(kAesTagLength, 16u);
}
