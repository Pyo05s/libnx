// 파일: main.cpp
// 생성일: 2026-02-23
// 설명: RTSP 통합 테스트 엔트리 포인트

#include <gtest/gtest.h>
#include <spdlog/spdlog.h>

int
main(int argc, char** argv)
{
  spdlog::set_level(spdlog::level::info);
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
