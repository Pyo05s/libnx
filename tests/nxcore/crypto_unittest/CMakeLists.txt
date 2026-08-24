# 프로젝트 이름 등록
set(PROJECT_NAME "nxcore_crypto_unittest")    

# 실행 파일 생성
file(GLOB SOURECES
    "*.cpp"
)

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
)

if(MSVC)
    # Release 전용
    target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/MD>)
endif()

# 테스트 등록
add_test(NAME ${PROJECT_NAME} COMMAND ${PROJECT_NAME})
