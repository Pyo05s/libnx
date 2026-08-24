 # CMakePresets.json 사용을 위해 3.20 이상 권장
 # Boost CMP0167 정책 무시를 위해 3.30 이상 필요
cmake_minimum_required(VERSION 3.30)
project(libnx LANGUAGES CXX)

if (${PROJECT_IS_TOP_LEVEL})
  set(CMAKE_CXX_STANDARD 23)
  set(CMAKE_CXX_STANDARD_REQUIRED ON)
  set(CMAKE_CXX_EXTENSIONS OFF)

  if(MSVC)
      # This adds the /Zi flag to all compile commands
      add_compile_options(/Zi /permissive- /EHsc)
      add_compile_definitions(_WIN32_WINNT=0x0A00)

      # Debug 전용
      add_compile_options("$<$<CONFIG:Debug>:/W4;/Od;/RTC1>")

      # Release 전용
      add_compile_options("$<$<CONFIG:Release>:/W4;/Oi;/Gy;/O2>")
      add_compile_definitions("$<$<CONFIG:Release>:NDEBUG>")
  endif()


  # 2. 빌드 결과물 모으기 (매우 중요)
  # 모든 실행 파일(.exe)과 동적 라이브러리(.dll)를 동일한 bin 폴더에 생성합니다.
  # 이렇게 하면 앱 실행 시 DLL을 찾지 못하는 경로 문제를 예방할 수 있습니다.
  set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
  set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
endif()


# 3. 외부 종속성 패키지 찾기
# vcpkg 등을 사용한다면 여기서 라이브러리를 미리 검사합니다.
find_package(spdlog REQUIRED)

# 옵션
option(ENABLE_LIBNX_NXCORE "Build nxcore" ON)
option(ENABLE_LIBNX_NXNET "Build nxnet" ON)
option(ENABLE_LIBNX_NXMEDIA "Build nxmedia" ON)
option(ENABLE_LIBNX_TEST "Build unit tests" ${PROJECT_IS_TOP_LEVEL})

if (ENABLE_LIBNX_NXCORE)
  add_subdirectory(nxcore)
endif()

if (ENABLE_LIBNX_NXNET)
  add_subdirectory(nxnet)
endif()

if (ENABLE_LIBNX_NXMEDIA)
  add_subdirectory(nxmedia)
endif()

if(ENABLE_LIBNX_TEST)
    enable_testing()    # CTest 기능을 활성화합니다.
    add_subdirectory(tests)
endif()

# 6. (선택) 빌드 완료 후 메시지 출력
message(STATUS "libnx Build Directory: ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}")