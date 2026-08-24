# 프로젝트 이름 등록
set(PROJECT_NAME "nxcore_file_unittest")

# 소스 파일 수집
file(GLOB SOURECES
    "*.cpp"
)

# 실행 파일 생성
add_executable(${PROJECT_NAME} ${SOURECES})

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

# 테스트 등록
add_test(NAME NxCoreFileTest COMMAND ${PROJECT_NAME})
