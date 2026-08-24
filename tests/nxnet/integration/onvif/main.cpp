// 파일: main.cpp
// 생성일: 2026-02-20
// 설명: ONVIF 통합 테스트 진입점

#include <gtest/gtest.h>
#include <iostream>

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);

  std::cout << "========================================\n";
  std::cout << "  ONVIF Integration Tests\n";
  std::cout << "========================================\n";
  std::cout << "  Camera: 192.168.0.168:80\n";
  std::cout << "  이 테스트는 실제 카메라 연결이 필요합니다.\n";
  std::cout << "========================================\n\n";

  return RUN_ALL_TESTS();
}
