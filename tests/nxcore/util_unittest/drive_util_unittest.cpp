// 파일: drive_util_unittest.cpp
// 생성일: 2026-03-25
// 설명: drive_util 드라이브 정보 조회 유틸리티 단위 테스트

#include <nxcore/util/drive_util.h>

#include <gtest/gtest.h>
#include <nlohmann/json.hpp>

namespace {

// ============================================================================
// enumerate_drives 테스트
// ============================================================================

TEST(DriveUtilTest, EnumerateDrives_ReturnsNonEmpty)
{
  // Windows 환경에서는 최소 1개 이상의 드라이브가 존재해야 함
  auto drives = nx::enumerate_drives();

#ifdef _WIN32
  ASSERT_FALSE(drives.empty()) << "Windows 환경에서 드라이브가 하나도 없음";
#endif
}

TEST(DriveUtilTest, EnumerateDrives_ContainsSystemDrive)
{
  auto drives = nx::enumerate_drives();

#ifdef _WIN32
  // C:\ 드라이브가 포함되어야 함
  bool has_c_drive = false;
  for (const auto& d : drives) {
    if (d.path == "C:\\" || d.path == "c:\\") {
      has_c_drive = true;
      break;
    }
  }
  EXPECT_TRUE(has_c_drive) << "시스템 드라이브(C:\\)가 목록에 없음";
#endif
}

TEST(DriveUtilTest, EnumerateDrives_ValidDriveInfo)
{
  auto drives = nx::enumerate_drives();

#ifdef _WIN32
  for (const auto& d : drives) {
    // 경로가 비어있지 않아야 함
    EXPECT_FALSE(d.path.empty()) << "드라이브 경로가 비어있음";

    // 타입은 fixed 또는 remote
    EXPECT_TRUE(d.type == "fixed" || d.type == "remote")
      << "드라이브 타입이 유효하지 않음: " << d.type;

    // 총 용량이 0보다 커야 함 (유효한 드라이브)
    EXPECT_GT(d.total_bytes, 0u) << "드라이브 " << d.path << ": total_bytes가 0";

    // used + free = total
    EXPECT_EQ(d.used_bytes + d.free_bytes, d.total_bytes)
      << "드라이브 " << d.path << ": used + free != total";

    // 사용률은 0~100 범위
    EXPECT_GE(d.usage_percent, 0.0) << "드라이브 " << d.path << ": 사용률이 음수";
    EXPECT_LE(d.usage_percent, 100.0)
      << "드라이브 " << d.path << ": 사용률이 100% 초과";
  }
#endif
}

TEST(DriveUtilTest, EnumerateDrives_OnlyFixedOrRemote)
{
  auto drives = nx::enumerate_drives();

  // CD-ROM, 이동식 디스크 등은 포함되지 않아야 함
  for (const auto& d : drives) {
    EXPECT_TRUE(d.type == "fixed" || d.type == "remote")
      << "필터링되지 않은 드라이브 타입: " << d.type << " path=" << d.path;
  }
}

// ============================================================================
// DriveInfo JSON 직렬화 테스트
// ============================================================================

TEST(DriveUtilTest, ToJson_AllFieldsPresent)
{
  nx::DriveInfo info{
    .path = "D:\\",
    .label = "Data",
    .type = "fixed",
    .total_bytes = 500'000'000'000,
    .free_bytes = 200'000'000'000,
    .used_bytes = 300'000'000'000,
    .usage_percent = 60.0};

  nlohmann::json j = info;

  EXPECT_EQ(j["path"], "D:\\");
  EXPECT_EQ(j["label"], "Data");
  EXPECT_EQ(j["type"], "fixed");
  EXPECT_EQ(j["total_bytes"], 500'000'000'000);
  EXPECT_EQ(j["free_bytes"], 200'000'000'000);
  EXPECT_EQ(j["used_bytes"], 300'000'000'000);
  EXPECT_EQ(j["usage_percent"], 60.0);
}

TEST(DriveUtilTest, ToJson_EmptyLabel)
{
  nx::DriveInfo info{
    .path = "E:\\",
    .label = "",
    .type = "remote",
    .total_bytes = 1'000'000'000,
    .free_bytes = 1'000'000'000,
    .used_bytes = 0,
    .usage_percent = 0.0};

  nlohmann::json j = info;

  EXPECT_EQ(j["label"], "");
  EXPECT_EQ(j["type"], "remote");
  EXPECT_EQ(j["used_bytes"], 0);
}

TEST(DriveUtilTest, ToJson_ArraySerialization)
{
  // 드라이브 목록을 JSON 배열로 변환
  std::vector<nx::DriveInfo> drives = {
    {.path = "C:\\",
     .label = "System",
     .type = "fixed",
     .total_bytes = 100,
     .free_bytes = 40,
     .used_bytes = 60,
     .usage_percent = 60.0},
    {.path = "D:\\",
     .label = "Data",
     .type = "fixed",
     .total_bytes = 200,
     .free_bytes = 150,
     .used_bytes = 50,
     .usage_percent = 25.0}
  };

  nlohmann::json arr = drives;

  ASSERT_EQ(arr.size(), 2);
  EXPECT_EQ(arr[0]["path"], "C:\\");
  EXPECT_EQ(arr[1]["path"], "D:\\");
}

TEST(DriveUtilTest, ToJson_FromEnumerateDrives)
{
  // 실제 enumerate_drives 결과를 JSON으로 변환
  auto drives = nx::enumerate_drives();

#ifdef _WIN32
  nlohmann::json arr = drives;

  ASSERT_EQ(arr.size(), drives.size());
  for (size_t i = 0; i < drives.size(); ++i) {
    EXPECT_EQ(arr[i]["path"], drives[i].path);
    EXPECT_EQ(arr[i]["total_bytes"], drives[i].total_bytes);
  }
#endif
}

// ============================================================================
// DriveInfo 기본값 테스트
// ============================================================================

TEST(DriveUtilTest, DriveInfo_DefaultValues)
{
  nx::DriveInfo info;

  EXPECT_TRUE(info.path.empty());
  EXPECT_TRUE(info.label.empty());
  EXPECT_TRUE(info.type.empty());
  EXPECT_EQ(info.total_bytes, 0u);
  EXPECT_EQ(info.free_bytes, 0u);
  EXPECT_EQ(info.used_bytes, 0u);
  EXPECT_EQ(info.usage_percent, 0.0);
}

} // namespace
