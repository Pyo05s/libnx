# 외부 종속성 패키지 찾기
find_package(GTest REQUIRED)

# nxcore 테스트 프로젝트
add_subdirectory(util_unittest)
add_subdirectory(record_unittest)
add_subdirectory(crypto_unittest)
add_subdirectory(media_unittest)
add_subdirectory(metrics_unittest)
add_subdirectory(file_unittest)
