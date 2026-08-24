// 파일: platform_util_unittest.cpp
// 생성일: 2026-04-28
// 설명: platform_util 프로세스 정보 및 워크스페이스 디렉터리 단위 테스트

#include <nxcore/util/platform_util.h>

#include <gtest/gtest.h>

#include <filesystem>

namespace {

// ============================================================================
// get_process_path 테스트
// ============================================================================

TEST(PlatformUtilTest, GetProcessPath_Succeeds)
{
  auto result = nx::util::get_process_path();
  ASSERT_TRUE(result.has_value()) << "에러: " << result.error().message();

  const auto& path = *result;
  EXPECT_FALSE(path.empty()) << "실행 파일 경로가 비어있음";
  EXPECT_TRUE(path.is_absolute()) << "실행 파일 경로가 절대 경로가 아님: " << path;
}

TEST(PlatformUtilTest, GetProcessPath_IsRegularFile)
{
  auto result = nx::util::get_process_path();
  ASSERT_TRUE(result.has_value());

  std::error_code ec;
  EXPECT_TRUE(std::filesystem::is_regular_file(*result, ec))
    << "경로가 일반 파일이 아님: " << *result;
  EXPECT_FALSE(ec) << "파일 상태 확인 중 오류: " << ec.message();
}

TEST(PlatformUtilTest, GetProcessPath_HasExecutableExtension)
{
  auto result = nx::util::get_process_path();
  ASSERT_TRUE(result.has_value());

#ifdef _WIN32
  auto ext = result->extension().string();
  EXPECT_EQ(ext, ".exe") << "Windows 실행 파일 확장자가 .exe 가 아님: " << ext;
#endif
}

// ============================================================================
// get_process_name 테스트
// ============================================================================

TEST(PlatformUtilTest, GetProcessName_Succeeds)
{
  auto result = nx::util::get_process_name();
  ASSERT_TRUE(result.has_value()) << "에러: " << result.error().message();

  EXPECT_FALSE(result->empty()) << "프로세스 이름이 비어있음";
}

TEST(PlatformUtilTest, GetProcessName_MatchesPathFilename)
{
  auto path_result = nx::util::get_process_path();
  auto name_result = nx::util::get_process_name();

  ASSERT_TRUE(path_result.has_value());
  ASSERT_TRUE(name_result.has_value());

  EXPECT_EQ(*name_result, path_result->filename().string())
    << "get_process_name()이 get_process_path().filename()과 일치하지 않음";
}

TEST(PlatformUtilTest, GetProcessName_DoesNotContainDirectorySeparator)
{
  auto result = nx::util::get_process_name();
  ASSERT_TRUE(result.has_value());

  const auto& name = *result;
  EXPECT_EQ(name.find('/'), std::string::npos)
    << "프로세스 이름에 '/' 포함됨: " << name;
  EXPECT_EQ(name.find('\\'), std::string::npos)
    << "프로세스 이름에 '\\\\' 포함됨: " << name;
}

// ============================================================================
// get_workspace_directory 테스트
// ============================================================================

TEST(PlatformUtilTest, GetWorkspaceDirectory_DefaultIsExeDirectory)
{
  // set_workspace_directory() 호출 전 기본값은 실행 파일 디렉터리여야 함
  auto process_path = nx::util::get_process_path();
  ASSERT_TRUE(process_path.has_value());

  auto expected_dir = process_path->parent_path();
  auto workspace_dir = nx::util::get_workspace_directory();

  EXPECT_FALSE(workspace_dir.empty()) << "워크스페이스 디렉터리가 비어있음";

  std::error_code ec;
  EXPECT_TRUE(std::filesystem::is_directory(workspace_dir, ec))
    << "기본 워크스페이스 디렉터리가 존재하지 않음: " << workspace_dir;

  // 정규화 후 비교
  auto canonical_workspace = std::filesystem::weakly_canonical(workspace_dir, ec);
  auto canonical_expected = std::filesystem::weakly_canonical(expected_dir, ec);
  EXPECT_EQ(canonical_workspace, canonical_expected)
    << "기본 워크스페이스가 실행 파일 디렉터리와 다름\n"
    << "  workspace: " << canonical_workspace << "\n"
    << "  expected:  " << canonical_expected;
}

// ============================================================================
// set_workspace_directory 테스트
// ============================================================================

class WorkspaceDirectoryTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 테스트 전 원래 CWD 저장
    std::error_code ec;
    m_original_cwd = std::filesystem::current_path(ec);
  }

  void TearDown() override
  {
    // 테스트 후 원래 CWD 복원 (다른 테스트에 영향 방지)
    if (!m_original_cwd.empty()) {
      std::error_code ec;
      std::filesystem::current_path(m_original_cwd, ec);
    }

    // 워크스페이스 디렉터리도 원래대로 복원 (TearDown에서는 에러 무시)
    if (!m_original_cwd.empty()) {
      (void)nx::util::set_workspace_directory(m_original_cwd);
    }
  }

  std::filesystem::path m_original_cwd;
};

TEST_F(WorkspaceDirectoryTest, SetWorkspaceDirectory_ValidPath_Succeeds)
{
  auto ec = nx::util::set_workspace_directory(m_original_cwd);
  EXPECT_FALSE(ec) << "유효한 경로 설정 실패: " << ec.message();
}

TEST_F(WorkspaceDirectoryTest, SetWorkspaceDirectory_UpdatesGetWorkspaceDirectory)
{
  // 임시 디렉터리를 워크스페이스로 설정
  auto temp_dir = std::filesystem::temp_directory_path();

  auto ec = nx::util::set_workspace_directory(temp_dir);
  ASSERT_FALSE(ec) << "set_workspace_directory 실패: " << ec.message();

  auto result_dir = nx::util::get_workspace_directory();
  auto canonical_temp = std::filesystem::weakly_canonical(temp_dir);
  auto canonical_result = std::filesystem::weakly_canonical(result_dir);

  EXPECT_EQ(canonical_result, canonical_temp)
    << "get_workspace_directory()가 설정한 경로를 반환하지 않음\n"
    << "  설정값: " << canonical_temp << "\n"
    << "  반환값: " << canonical_result;
}

TEST_F(WorkspaceDirectoryTest, SetWorkspaceDirectory_UpdatesOsCwd)
{
  auto temp_dir = std::filesystem::temp_directory_path();

  auto ec = nx::util::set_workspace_directory(temp_dir);
  ASSERT_FALSE(ec) << "set_workspace_directory 실패: " << ec.message();

  // OS CWD도 변경되었는지 확인
  std::error_code fs_ec;
  auto cwd = std::filesystem::current_path(fs_ec);
  ASSERT_FALSE(fs_ec);

  auto canonical_temp = std::filesystem::weakly_canonical(temp_dir);
  auto canonical_cwd = std::filesystem::weakly_canonical(cwd);

  EXPECT_EQ(canonical_cwd, canonical_temp) << "OS CWD가 설정한 경로와 다름\n"
                                           << "  설정값: " << canonical_temp << "\n"
                                           << "  OS CWD: " << canonical_cwd;
}

TEST_F(WorkspaceDirectoryTest, SetWorkspaceDirectory_NonExistentPath_ReturnsError)
{
  std::filesystem::path nonexistent = m_original_cwd / "nonexistent_path_xyz_12345";

  auto ec = nx::util::set_workspace_directory(nonexistent);
  EXPECT_TRUE(ec) << "존재하지 않는 경로에 대해 에러가 반환되어야 함";
  EXPECT_EQ(ec, std::make_error_code(std::errc::no_such_file_or_directory))
    << "에러 코드가 예상과 다름: " << ec.message();
}

TEST_F(WorkspaceDirectoryTest, SetWorkspaceDirectory_NonExistentPath_DoesNotChangeCwd)
{
  std::filesystem::path nonexistent = m_original_cwd / "nonexistent_path_xyz_12345";

  // 실패해도 CWD가 변경되지 않아야 함
  std::error_code ec_before;
  auto cwd_before = std::filesystem::current_path(ec_before);
  ASSERT_FALSE(ec_before);

  // 의도적으로 반환값 사용 안 함 — CWD 불변 여부 확인이 목적
  auto unused_ec = nx::util::set_workspace_directory(nonexistent);
  (void)unused_ec;

  std::error_code ec_after;
  auto cwd_after = std::filesystem::current_path(ec_after);
  ASSERT_FALSE(ec_after);

  EXPECT_EQ(cwd_before, cwd_after) << "실패한 set_workspace_directory가 CWD를 변경함";
}

} // namespace
