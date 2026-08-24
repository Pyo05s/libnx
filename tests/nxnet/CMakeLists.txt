# 외부 종속성 패키지 찾기
find_package(GTest REQUIRED)

# nxnet 테스트 프로젝트        
add_subdirectory(unittest)
add_subdirectory(integration)
