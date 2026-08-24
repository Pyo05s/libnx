# 프로젝트 이름 등록
set(PROJECT_NAME "libnxcore")

# 라이브러리 파일 정의
file(GLOB SOURECES
    "util/*.h"
    "util/*.cpp"
    "string/*.h"
    "string/*.cpp"
    "record/*.h"
    "record/*.cpp"
    "media/*.h"
    "media/*.cpp"
    "crypto/*.h"
    "crypto/*.cpp"
    "metrics/*.h"
    "metrics/*.cpp"
    "file/*.h"
    "file/*.cpp"
    "device/*.h"
    "device/*.cpp")

# 라이브러리 타겟 생성
add_library(${PROJECT_NAME} STATIC ${SOURECES})

# nxcore를 링크하는 타겟들이 참조할 include 경로 설정
# PROJECT_SOURCE_DIR는 프로젝트 루트인 'HiVe2/'를 의미하게 됩니다.
target_include_directories(${PROJECT_NAME} PUBLIC "${PROJECT_SOURCE_DIR}")

# vcpkg 패키지 찾기
find_package(nlohmann_json CONFIG REQUIRED)
find_package(Boost REQUIRED COMPONENTS system)
find_package(spdlog CONFIG REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(pugixml CONFIG REQUIRED)

# 외부 라이브러리 링크
target_link_libraries(${PROJECT_NAME} PUBLIC
    Boost::system
    OpenSSL::Crypto
    pugixml::pugixml
)

target_link_libraries(${PROJECT_NAME} PUBLIC
    nlohmann_json::nlohmann_json
    spdlog::spdlog
)
