# 프로젝트 이름 등록
set(PROJECT_NAME "libnxmedia")

# 라이브러리 파일 정의
file(GLOB SOURECES
    "file/*.h"
    "file/*.cpp"
    "codec/*.h"
    "codec/*.cpp"
)

# 라이브러리 타겟 생성
add_library(${PROJECT_NAME} STATIC ${SOURECES})

# nxcore를 링크하는 타겟들이 참조할 include 경로 설정
# PROJECT_SOURCE_DIR는 프로젝트 루트인 'HiVe2/'를 의미하게 됩니다.
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}")

# vcpkg 패키지 찾기
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(FFMPEG REQUIRED MODULE COMPONENTS avformat avcodec avutil swscale swresample)

# 외부 라이브러리 링크
target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::system
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)

# ${FFMPEG_LIBRARIES}, ${FFMPEG_INCLUDE_DIRS} 변수는
# vcpkg의 share/ffmpeg/FindFFMPEG.cmake 모듈에서 제공됩니다.
target_link_libraries(${PROJECT_NAME} PRIVATE
    ${FFMPEG_LIBRARIES}
)

target_include_directories(${PROJECT_NAME} PRIVATE ${FFMPEG_INCLUDE_DIRS})