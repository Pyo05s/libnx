# nxnet 통합 테스트 (httpbin.org)

이 디렉터리에는 **httpbin.org**를 이용한 `nxnet/auth` 모듈의 통합 테스트가 포함되어 있습니다.

## 목적

단위 테스트에서 RFC 표준 테스트 벡터로 검증한 인증 구현이 실제 HTTP 서버와 정상적으로 동작하는지 확인합니다.

## 테스트 범위

### ✅ 검증 가능한 인증 방식

| 인증 방식   | 테스트 파일                    | httpbin.org 엔드포인트                         | 주요 테스트                                                          |
| ----------- | ------------------------------ | ---------------------------------------------- | -------------------------------------------------------------------- |
| **Basic**   | `basic_auth_httpbin_test.cpp`  | `/basic-auth/{user}/{pass}`                    | 정상 인증, 잘못된 자격 증명, 특수 문자, 헤더 캐싱                    |
| **Digest**  | `digest_auth_httpbin_test.cpp` | `/digest-auth/{qop}/{user}/{pass}/{algorithm}` | Challenge-Response 흐름, MD5/SHA-256, qop=auth/auth-int, nonce count |
| **Bearer**  | `bearer_auth_httpbin_test.cpp` | `/bearer`                                      | 토큰 전송, JWT 형식, 긴 토큰, 특수 문자                              |
| **API Key** | `apikey_auth_httpbin_test.cpp` | `/headers`                                     | X-API-Key 헤더, 커스텀 헤더, Authorization 헤더, 특수 문자           |

### ❌ 별도 환경 필요

- **WS-Security (ONVIF/SOAP)**: httpbin.org는 SOAP을 지원하지 않으므로 별도 Docker 시뮬레이터 필요

## 빌드 및 실행

### 1. 빌드

```powershell
# 프로젝트 루트에서
cmake --build build/x64-debug --target nxnet_integration_test
```

### 2. 실행

```powershell
# 빌드 디렉터리에서
.\build\x64-debug\bin\nxnet_integration_test.exe

# 또는 CTest로 실행
cd build/x64-debug
ctest -R nxnet_integration_test -V
```

### 3. 특정 테스트만 실행

```powershell
# Basic 인증 테스트만
.\build\x64-debug\bin\nxnet_integration_test.exe --gtest_filter="HttpbinIntegrationTest.BasicAuth*"

# Digest 인증 테스트만
.\build\x64-debug\bin\nxnet_integration_test.exe --gtest_filter="HttpbinIntegrationTest.DigestAuth*"
```

## 네트워크 요구사항

### ⚠️ 필수 조건

- **인터넷 연결 필요**: httpbin.org에 접속할 수 있어야 합니다.
- **방화벽 설정**: HTTP (포트 80) 아웃바운드 연결 허용

### CI/CD 환경 설정

네트워크가 불가능한 CI 환경에서 테스트를 스킵하려면:

```powershell
# 환경 변수 설정
$env:SKIP_NETWORK_TESTS = "1"
ctest -R nxnet_integration_test
```

CMake 설정에서 자동으로 테스트를 비활성화합니다.

## 테스트 구조

```
integration/
├── common/
│   ├── httpbin_test_fixture.h      # 공통 테스트 픽스처
│   └── test_config.h                # httpbin.org 설정
│
├── basic_auth_httpbin_test.cpp      # Basic 인증 (6개 테스트)
├── digest_auth_httpbin_test.cpp     # Digest 인증 (4개 테스트)
├── bearer_auth_httpbin_test.cpp     # Bearer 토큰 (4개 테스트)
├── apikey_auth_httpbin_test.cpp     # API Key (5개 테스트)
├── main.cpp                          # Google Test 진입점
├── CMakeLists.txt                    # 빌드 설정
└── README.md                         # 이 문서
```

## 아키텍처

### AuthProvider 인터페이스

```cpp
class AuthProvider {
public:
    // 헤더 전체(key, value) 생성 - HttpClient가 사용
    virtual nx::expected<boost::beast::http::fields>
        generate_headers(const AuthContext& context) const;
};
```

**설계 원칙:**

1. **Provider가 헤더 제어**: 헤더 이름과 값 모두 Provider가 결정
2. **HttpClient는 결과 사용**: Provider 결과를 그대로 HTTP 요청에 추가
3. **하위 호환성**: 기본 구현은 `Authorization` 헤더 사용
4. **확장성**: API Key 등 커스텀 헤더는 오버라이드

### 테스트 플로우

```cpp
// httpbin_test_fixture.h
auto headers_result = auth_provider->generate_headers(auth_ctx);

nx::net::HttpRequest request{
    .method = method,
    .target = target,
    .body = body,
    .headers = *headers_result  // Provider가 결정한 헤더 사용
};
```

**장점:**

- ✅ Basic/Digest/Bearer: `{"Authorization": "..."}`
- ✅ API Key: `{"X-API-Key": "..."}` 또는 커스텀 헤더
- ✅ HTTP Client는 인증 세부사항 불필요

## 테스트 결과

### ✅ 수행 완료 (19개 테스트)

| 인증 방식 | 테스트 수 | 상태        |
| --------- | --------- | ----------- |
| Basic     | 6         | ✅ PASSED   |
| Digest    | 4         | ✅ PASSED   |
| Bearer    | 4         | ✅ PASSED   |
| API Key   | 5         | ✅ PASSED   |
| **합계**  | **19**    | **✅ 100%** |

```
├── digest_auth_httpbin_test.cpp     # Digest 인증 (8개 테스트)
├── bearer_auth_httpbin_test.cpp     # Bearer 토큰 (7개 테스트)
└── apikey_auth_httpbin_test.cpp     # API Key (8개 테스트)
```

### 공통 픽스처 (`HttpbinIntegrationTest`)

모든 테스트는 `HttpbinIntegrationTest` 클래스를 상속받아 다음 기능을 사용합니다:

- `connect_httpbin()`: httpbin.org 연결
- `send_authenticated_request()`: 인증 헤더 포함 요청
- `send_unauthenticated_request()`: 인증 없는 요청 (401 Challenge 수신용)
- `m_client`: `HttpClient` 인스턴스
- `m_runner`: `IoContextTestRunner` (비동기 테스트 실행)

## 주요 테스트 시나리오

### Basic 인증

1. **정상 인증**: 올바른 자격 증명으로 200 OK 응답
2. **잘못된 자격 증명**: 401 Unauthorized + WWW-Authenticate 헤더
3. **특수 문자**: 이메일, 특수 기호 포함 자격 증명
4. **헤더 재사용**: 동일 인증 객체로 여러 요청

### Digest 인증

1. **Challenge-Response 흐름**:
   - 401 Challenge 수신 → 파싱 → Response 계산 → 인증 성공
2. **알고리즘**: MD5, SHA-256
3. **QoP**: auth, auth-int
4. **Nonce Count**: 동일 nonce로 여러 요청 시 nc 증가
5. **Stale Nonce**: nonce 만료 처리
6. **복제 테스트**: `clone()` 메서드로 인증 객체 복제

### Bearer 토큰

1. **토큰 전송**: httpbin.org가 토큰 에코 확인
2. **JWT 형식**: 실제 JWT 형식 토큰 전송 (검증 없음)
3. **긴 토큰**: 1000자 이상 토큰
4. **특수 문자**: Base64 문자 포함

### API Key

1. **기본 헤더**: X-API-Key
2. **커스텀 헤더**: 임의의 헤더 이름
3. **Authorization 헤더**: ApiKey 형식
4. **특수 문자**: 하이픈, 언더스코어 등

## 타임아웃 설정

| 항목                 | 기본값 | 설정 위치                         |
| -------------------- | ------ | --------------------------------- |
| 연결 타임아웃        | 10초   | `test_config.h::kConnectTimeout`  |
| 응답 타임아웃        | 30초   | `test_config.h::kResponseTimeout` |
| 테스트 전체 타임아웃 | 60초   | `test_config.h::kTestTimeout`     |
| CTest 타임아웃       | 180초  | `CMakeLists.txt`                  |

네트워크 상태에 따라 필요 시 조정 가능합니다.

## 검증 방법

### httpbin.org의 역할

httpbin.org는 Kenneth Reitz가 만든 **표준 HTTP 테스트 서비스**로:

- RFC 표준 준수 (Basic, Digest 인증)
- 수백만 개발자가 사용 중
- 오픈소스 (Python Flask 기반)

### 신뢰성 보장

1. **RFC 표준 벡터** (단위 테스트) + **httpbin.org** (통합 테스트) 조합
2. httpbin.org가 인증을 승인하면 → 실제 서버와 호환 보장
3. 잘못된 구현은 httpbin.org가 거부 (401 응답)

### 예시: Digest 인증

```
[우리 구현]                    [httpbin.org]
    ↓                              ↓
Challenge 파싱 ──────────→ RFC 준수 Challenge 생성
    ↓                              ↓
Response 계산 ────────────→ RFC 준수 Response 검증
    ↓                              ↓
200 OK ←──────────────────── 검증 성공
```

만약 우리 구현이 RFC와 다르면 → httpbin.org가 401 반환 → 테스트 실패

## 문제 해결

### 네트워크 오류

```
httpbin.org 연결 실패: Connection timed out
```

**해결 방법:**

- 인터넷 연결 확인
- 방화벽/프록시 설정 확인
- httpbin.org 서비스 상태 확인 ([httpbin.org](https://httpbin.org/))

### 테스트 실패

```
예상: 200 OK, 실제: 401 Unauthorized
```

**원인:**

- 인증 헤더 생성 오류
- Challenge 파싱 오류
- RFC 표준 불일치

**디버깅:**

1. 단위 테스트 확인 (`nxnet_unittest`)
2. Wireshark로 실제 HTTP 헤더 확인
3. httpbin.org 응답 본문 확인 (오류 메시지 포함)

## 향후 계획

- [ ] **WS-Security 통합 테스트**: Docker ONVIF 시뮬레이터 추가
- [ ] **HTTPS 테스트**: httpbin.org의 HTTPS 엔드포인트 사용
- [ ] **퍼포먼스 테스트**: 대량 요청 시 성능 측정
- [ ] **재연결 테스트**: 네트워크 단절 후 재연결

## 참고 자료

- [httpbin.org](https://httpbin.org/) - HTTP 테스트 서비스
- [RFC 7617](https://tools.ietf.org/html/rfc7617) - HTTP Basic 인증
- [RFC 7616](https://tools.ietf.org/html/rfc7616) - HTTP Digest 인증
- [RFC 6750](https://tools.ietf.org/html/rfc6750) - OAuth 2.0 Bearer Token
