# RTSP 통합 테스트
# 주의: 이 테스트는 실제 카메라 연결이 필요합니다.

# 테스트 실행 파일 생성
add_executable(nxnet_rtsp_integration
    main.cpp
    rtsp_client_integration_test.cpp
)

# 포함 디렉터리
target_include_directories(nxnet_rtsp_integration
    PRIVATE
        ${CMAKE_SOURCE_DIR}
        ${CMAKE_SOURCE_DIR}/tests
)

# 링크 라이브러리
target_link_libraries(nxnet_rtsp_integration
    PRIVATE
        libnxnet
        libnxcore
        test_common
        GTest::gtest
)

# 컴파일러 옵션
if(MSVC)
    target_compile_options(nxnet_rtsp_integration PRIVATE /W4)
else()
    target_compile_options(nxnet_rtsp_integration PRIVATE -Wall -Wextra -Wpedantic)
endif()

# C++23 표준
target_compile_features(nxnet_rtsp_integration PRIVATE cxx_std_23)

# 테스트 등록
add_test(
    NAME nxnet_rtsp_integration
    COMMAND nxnet_rtsp_integration
)

# 테스트 속성
set_tests_properties(nxnet_rtsp_integration
    PROPERTIES
        LABELS "integration;rtsp;network"
        TIMEOUT 180
)
