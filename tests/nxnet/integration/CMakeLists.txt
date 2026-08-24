# nxnet 통합 테스트 (httpbin.org 기반)
# 주의: 이 테스트는 인터넷 연결이 필요합니다.

# 테스트 실행 파일 생성
add_executable(nxnet_integration_test
    main.cpp
    basic_auth_httpbin_test.cpp
    digest_auth_httpbin_test.cpp
    bearer_auth_httpbin_test.cpp
    apikey_auth_httpbin_test.cpp
)

# 포함 디렉터리
target_include_directories(nxnet_integration_test
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
)

# 링크 라이브러리
target_link_libraries(nxnet_integration_test
    PRIVATE
        libnxnet             # HTTP 클라이언트
        libnxcore            # 유틸리티
        GTest::gtest
        GTest::gtest_main
)

# 컴파일러 옵션
if(MSVC)
    target_compile_options(nxnet_integration_test PRIVATE /W4)
else()
    target_compile_options(nxnet_integration_test PRIVATE -Wall -Wextra -Wpedantic)
endif()

# C++23 표준 사용
target_compile_features(nxnet_integration_test PRIVATE cxx_std_23)

# 테스트 등록
add_test(
    NAME nxnet_integration_test
    COMMAND nxnet_integration_test
)

# 테스트 속성 설정
set_tests_properties(nxnet_integration_test
    PROPERTIES
        LABELS "integration;network"
        TIMEOUT 180  # 3분 타임아웃 (네트워크 지연 고려)
)

# 네트워크 없이 빌드 시 테스트 스킵 가능하도록 옵션 설정
# CI 환경 변수가 설정되어 있고 네트워크가 없으면 스킵
if(DEFINED ENV{CI} AND DEFINED ENV{SKIP_NETWORK_TESTS})
    set_tests_properties(nxnet_integration_test
        PROPERTIES
            DISABLED TRUE
    )
    message(STATUS "nxnet_integration_test will be skipped (SKIP_NETWORK_TESTS is set)")
endif()

# RTSP 통합 테스트
add_subdirectory(rtsp)

# 테스트 출력 상세화
set_tests_properties(nxnet_integration_test
    PROPERTIES
        ENVIRONMENT "GTEST_COLOR=1"
)

# ONVIF 통합 테스트
add_subdirectory(onvif)
