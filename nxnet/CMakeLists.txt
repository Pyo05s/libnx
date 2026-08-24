# 프로젝트 이름 등록
set(PROJECT_NAME "libnxnet")

# 라이브러리 파일 정의
file(GLOB SOURECES
    "http/*.h"
    "http/*.cpp"
    "http/detail/*.h"
    "http/detail/*.cpp"
    "onvif/*.h"
    "onvif/*.cpp"
    "onvif/soap/*.h"
    "onvif/soap/*.cpp"
    "onvif/services/*.h"
    "onvif/services/*.cpp"
    "auth/*.h"
    "auth/*.cpp"
    "auth/basic/*.h"
    "auth/basic/*.cpp"
    "auth/digest/*.h"
    "auth/digest/*.cpp"
    "auth/bearer/*.h"
    "auth/bearer/*.cpp"
    "auth/apikey/*.h"
    "auth/apikey/*.cpp"
    "auth/ws_security/*.h"
    "auth/ws_security/*.cpp"
    "rtsp/*.h"
    "rtsp/*.cpp"
    "rtp/*.h"
    "rtp/*.cpp"
    "sdp/*.h"
    "sdp/*.cpp")

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
    libnxcore          # nxcore 유틸리티 (type_util.h 등)
    libnxmedia         # nxmedia 코덱 유틸리티 (jpeg_parser 등)
    Boost::system
    nlohmann_json::nlohmann_json
    spdlog::spdlog
    OpenSSL::SSL
    OpenSSL::Crypto
    pugixml::pugixml
)

# MSVC 컴파일러 옵션
if(MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE /bigobj)
endif()
