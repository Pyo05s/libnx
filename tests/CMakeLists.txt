# Common Interface 등록
add_library(test_common INTERFACE)

# 이 라이브러리를 쓰는 타겟들이 가져갈 헤더 경로 설정
target_include_directories(test_common INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/common")

# 외부 종속성 패키지 찾기
find_package(GTest REQUIRED)

# nxcore 테스트 프로젝트        
add_subdirectory(nxcore)
add_subdirectory(nxnet)
add_subdirectory(nxmedia)

