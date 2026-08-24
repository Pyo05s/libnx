# 프로젝트 이름 등록
set(PROJECT_NAME "nxcore_media_unittest")    

# 1. 실행 파일 생성 (테스트 파일만 포함, nxcore 소스는 라이브러리로 링크)
file(GLOB SOURECES
    "*.cpp"
)

add_executable(${PROJECT_NAME} ${SOURECES})

# 2. 의존성 연결 (nxcore 라이브러리 + GTest)
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    libnxcore      # 자체 제작 라이브러리
    test_common    # 공통 헤더 인터페이스 라이브러리
    GTest::gtest   # GTest
)

# Include 디렉토리 설정
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${PROJECT_SOURCE_DIR}"
)

if(MSVC)
endif()

# 3. 테스트 등록 (ctest를 통해 실행 가능하게 함)
add_test(NAME NxCoreMediaTest COMMAND ${PROJECT_NAME})
