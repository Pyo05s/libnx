// 파일: test_config.h
// 생성일: 2026-02-10
// 설명: httpbin.org 통합 테스트 설정

#pragma once

#include <nxcore/util/time_util.h>

namespace test::httpbin {

// httpbin.org 서버 설정
// inline constexpr const char* kHttpbinHost = "httpbin.org";
// inline constexpr const char* kHttpbinHost = "httpbun.com";
inline constexpr const char* kHttpbinHost = "127.0.0.1"; // 로컬 httpbin 서버 사용

// HTTPS 로 rediect 하기 때문에 HTTP 테스트에서 사용 불가
// inline constexpr const char* kHttpbinHost = "mockhttp.org";
inline constexpr uint16_t kHttpbinPort = 80;

// 타임아웃 설정
inline constexpr nx::milliseconds kConnectTimeout{10000};   // 10초
inline constexpr nx::milliseconds kResponseTimeout{300000}; // 30초
inline constexpr nx::milliseconds kTestTimeout{600000};     // 테스트 전체 타임아웃 60초

// API 엔드포인트
inline constexpr const char* kBasicAuthEndpoint = "/basic-auth";
inline constexpr const char* kHiddenBasicAuthEndpoint = "/hidden-basic-auth";
inline constexpr const char* kDigestAuthEndpoint = "/digest-auth";
inline constexpr const char* kBearerEndpoint = "/bearer";
inline constexpr const char* kHeadersEndpoint = "/headers";
inline constexpr const char* kAnythingEndpoint = "/anything";

// 테스트 자격 증명
inline constexpr const char* kTestUser = "testuser";
inline constexpr const char* kTestPass = "testpass";

} // namespace test::httpbin
