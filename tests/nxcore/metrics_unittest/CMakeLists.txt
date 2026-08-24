# 프로젝트 이름 등록
set(PROJECT_NAME "nxcore_metrics_unittest")

# 실행 파일 생성
file(GLOB SOURCES
    "*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURCES})

# 의존성 연결
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    libnxcore
    GTest::gtest
)

# Include 디렉토리 설정
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/common"
)

if(MSVC)
    target_compile_definitions(${PROJECT_NAME} PRIVATE _CRT_SECURE_NO_WARNINGS)
endif()

# 테스트 등록
add_test(NAME NxCoreMetricsTest COMMAND ${PROJECT_NAME})
