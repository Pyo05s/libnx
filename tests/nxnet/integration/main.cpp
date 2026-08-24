// 파일: main.cpp
// 생성일: 2026-02-10
// 설명: nxnet 통합 테스트 진입점

#include <gtest/gtest.h>

#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/spdlog.h>

#include <iostream>

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  std::cout << "========================================\n";
  std::cout << "nxnet Integration Tests (httpbin.org)\n";
  std::cout << "========================================\n";
  std::cout << "주의: 이 테스트는 인터넷 연결이 필요합니다.\n";
  std::cout << "      네트워크 문제 시 테스트가 실패할 수 있습니다.\n";
  std::cout << "========================================\n\n";

  std::vector<spdlog::sink_ptr> sinks;
  sinks.push_back(std::make_shared<spdlog::sinks::stdout_color_sink_mt>());

  auto logger = std::make_shared<spdlog::logger>("unittest", sinks.begin(), sinks.end());

  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::debug);

  return RUN_ALL_TESTS();
}
