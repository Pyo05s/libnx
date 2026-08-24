// 파일: main.cpp
// 생성일: 2026-02-10
// 설명: crypto 모듈 테스트 메인 함수

#include <iostream>
#include <gtest/gtest.h>

#include <spdlog/spdlog.h>
#include <spdlog/sinks/stdout_color_sinks.h>
#include <spdlog/sinks/rotating_file_sink.h>

int
main(int argc, char** argv)
{
  // 콘솔 + 파일 로그 설정
  auto console_sink = std::make_shared<spdlog::sinks::stdout_color_sink_mt>();
  // auto file_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
  //     "logs/recorder.log", 1024 * 1024 * 10, 3);

  // std::vector<spdlog::sink_ptr> sinks{ console_sink, file_sink };
  std::vector<spdlog::sink_ptr> sinks{console_sink};
  auto logger
    = std::make_shared<spdlog::logger>("recorder", sinks.begin(), sinks.end());

  spdlog::set_default_logger(logger);
  spdlog::set_level(spdlog::level::debug); // 개발 시 debug, 프로덕션 시 info
  spdlog::set_pattern("[%Y-%m-%d %H:%M:%S.%e] [%^%l%$] [%t] %v");

  ::testing::InitGoogleTest(&argc, argv);

  return RUN_ALL_TESTS();
}
