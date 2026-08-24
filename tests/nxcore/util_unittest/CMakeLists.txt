# 프로젝트 이름 등록
set(PROJECT_NAME "nxcore_util_unittest")    

# 1. 실행 파일 생성 (테스트 파일만 포함, nxcore 소스는 라이브러리로 링크)
file(GLOB SOURECES
    "*.cpp"
)

# 경로 이동 없이 부모 디렉토리의 파일을 참조할 때는 ../.. 형식을 사용합니다.
add_executable(${PROJECT_NAME} ${SOURECES} )

# 2. 의존성 연결 (nxcore 라이브러리 + GTest)
# nxcore는 루트에서 add_subdirectory로 등록되어 있어야 이름으로 호출 가능합니다.
target_link_libraries(${PROJECT_NAME}
    PRIVATE
    libnxcore      # 자체 제작 라이브러리
    GTest::gtest   # GTest (main이 포함된 버전이나 gtest 직접 연결)
)

# Include 디렉토리 설정
# nxcore 헤더를 포함하기 위해 프로젝트 루트 디렉토리를 추가합니다.
target_include_directories(${PROJECT_NAME}
    PRIVATE
    "${PROJECT_SOURCE_DIR}"
    "${PROJECT_SOURCE_DIR}/tests/common"
)

if(MSVC)
    # 아래 옵션들은 기본값이므로 주석 처리했습니다.
    # # Debug 전용
    # target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Debug>:/MDd>)

    # # Release 전용
    # target_compile_options(${PROJECT_NAME} PRIVATE $<$<CONFIG:Release>:/MD>)
endif()

# 3. 테스트 등록 (ctest를 통해 실행 가능하게 함)
add_test(NAME NxCoreUtilTest COMMAND ${PROJECT_NAME})

# 4. 빌드 후 출력 위치 확인 (선택 사항)
# 루트 CMakeLists.txt의 설정에 따라 bin 폴더로 모이게 됩니다.