# C++ Onvif Reader with boost::asio

## 구현

1. C++23 Base
2. boost asio 및 coroutine 사용

## 라이브러리

1. spdlog: 1.17.0
2. boost: 1.90.0
3. GTest: 1.17.0
4. OpenSSL: 3.6.1
5. nlohmann-json: 3.12.0

## VCPKG

    vcpkg 를 사용하지 않는다면 종속성 라이브러리를 수동 설치 해야 함

1. VCPKG_ROOT 환경 변수 등록 필수
   - CMakePresets.json > CMAKE_TOOLCHAIN_FILE 에 기록
2. cmake vcpkg manifast mode 사용 X

## Tests

1. nxnet_unittest 는 외부 사이트를 사용합니다.
   - HTTPS 테스트: mockhttp.org
   - HTTP 테스트: "127.0.0.1" 원래 httpbin.org 를 사용했으나 사이트 상태에 따라 실패 하는 경우도 있기에 Docker 로 kennethreitz/httpbin 을 구동해서 테스트 함.
