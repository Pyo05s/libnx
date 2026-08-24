// 파일: file_stream_unittest.cpp
// 생성일: 2026-05-22
// 설명: nx::file::FileStream 단위 테스트

#include <nxcore/file/file_stream.h>

#include <gtest/gtest.h>

#include <array>
#include <cstring>
#include <filesystem>
#include <vector>

namespace fs = std::filesystem;

namespace {

/// 테스트 임시 디렉토리 기반 경로 생성 헬퍼
fs::path
make_temp_path(std::string_view filename)
{
  static const fs::path kTempDir = fs::temp_directory_path() / "nxcore_file_unittest";
  fs::create_directories(kTempDir);
  return kTempDir / filename;
}

} // namespace

// ─────────────────────────────────────────────────────────
// 테스트 픽스처
// ─────────────────────────────────────────────────────────

class FileStreamTest : public ::testing::Test
{
protected:
  void TearDown() override
  {
    // 테스트 후 임시 파일 정리
    for (const auto& path : m_temp_files) {
      std::error_code ec;
      fs::remove(path, ec);
    }
  }

  /// 임시 파일 경로 등록 및 반환
  fs::path temp_file(std::string_view name)
  {
    auto path = make_temp_path(name);
    m_temp_files.push_back(path);
    return path;
  }

  std::vector<fs::path> m_temp_files;
};

// ─────────────────────────────────────────────────────────
// 테스트 1: 기본 Write / Read
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, BasicWriteRead)
{
  const auto path = temp_file("basic_write_read.bin");
  const std::vector<uint8_t> kData = {0x01, 0x02, 0x03, 0x04, 0x05};

  // 쓰기
  {
    nx::file::FileStream writer;
    auto ec = writer.open(path, nx::file::OpenMode::kWriteTruncate);
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_TRUE(writer.is_open());

    auto result = writer.write(kData);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, kData.size());
  }

  // 읽기
  {
    nx::file::FileStream reader;
    auto ec = reader.open(path, nx::file::OpenMode::kRead);
    ASSERT_FALSE(ec) << ec.message();
    ASSERT_TRUE(reader.is_open());

    std::vector<uint8_t> buf(kData.size());
    auto result = reader.read(buf);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, kData.size());
    EXPECT_EQ(buf, kData);
  }
}

// ─────────────────────────────────────────────────────────
// 테스트 2: Seek (Begin / Current / End)
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, SeekOrigins)
{
  const auto path = temp_file("seek_test.bin");
  const std::vector<uint8_t> kData = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE};

  // 데이터 준비
  {
    nx::file::FileStream w;
    ASSERT_FALSE(w.open(path, nx::file::OpenMode::kWriteTruncate));
    ASSERT_TRUE(w.write(kData).has_value());
  }

  nx::file::FileStream f;
  ASSERT_FALSE(f.open(path, nx::file::OpenMode::kRead));

  // kBegin: 위치 0으로 이동
  auto pos = f.seek(0, nx::file::SeekOrigin::kBegin);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(*pos, 0);

  // kCurrent: 현재 위치에서 2 이동
  pos = f.seek(2, nx::file::SeekOrigin::kCurrent);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(*pos, 2);

  // tell() 확인
  auto tell_pos = f.tell();
  ASSERT_TRUE(tell_pos.has_value());
  EXPECT_EQ(*tell_pos, 2);

  // kEnd: 파일 끝에서 -1 이동 → 마지막 바이트
  pos = f.seek(-1, nx::file::SeekOrigin::kEnd);
  ASSERT_TRUE(pos.has_value());
  EXPECT_EQ(*pos, static_cast<int64_t>(kData.size()) - 1);

  // 그 위치에서 1바이트 읽기
  uint8_t byte = 0;
  auto result = f.read({&byte, 1});
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(byte, 0xEE);
}

// ─────────────────────────────────────────────────────────
// 테스트 3: write_struct / read_struct 왕복
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, WriteReadStruct)
{
  struct TestData
  {
    uint32_t a;
    uint16_t b;
    uint8_t c;
  };

  const auto path = temp_file("struct_rw.bin");
  const TestData kWrite = {0xDEADBEEF, 0x1234, 0xAB};

  // 쓰기
  {
    nx::file::FileStream w;
    ASSERT_FALSE(w.open(path, nx::file::OpenMode::kWriteTruncate));
    auto ec = w.write_struct(kWrite);
    EXPECT_FALSE(ec) << ec.message();
  }

  // 읽기
  {
    nx::file::FileStream r;
    ASSERT_FALSE(r.open(path, nx::file::OpenMode::kRead));

    TestData out{};
    EXPECT_TRUE(r.read_struct(out));
    EXPECT_EQ(out.a, kWrite.a);
    EXPECT_EQ(out.b, kWrite.b);
    EXPECT_EQ(out.c, kWrite.c);
  }
}

// ─────────────────────────────────────────────────────────
// 테스트 4: Append 모드
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, AppendMode)
{
  const auto path = temp_file("append_test.bin");
  const std::vector<uint8_t> kFirst = {0x01, 0x02};
  const std::vector<uint8_t> kSecond = {0x03, 0x04};

  // 첫 번째 쓰기
  {
    nx::file::FileStream w;
    ASSERT_FALSE(w.open(path, nx::file::OpenMode::kWriteTruncate));
    ASSERT_TRUE(w.write(kFirst).has_value());
  }

  // Append 모드로 추가 쓰기
  {
    nx::file::FileStream w;
    ASSERT_FALSE(w.open(path, nx::file::OpenMode::kAppend));
    ASSERT_TRUE(w.write(kSecond).has_value());
  }

  // 전체 내용 검증
  {
    nx::file::FileStream r;
    ASSERT_FALSE(r.open(path, nx::file::OpenMode::kRead));

    std::vector<uint8_t> buf(4);
    auto result = r.read(buf);
    ASSERT_TRUE(result.has_value());
    EXPECT_EQ(*result, 4u);
    EXPECT_EQ(buf[0], 0x01);
    EXPECT_EQ(buf[1], 0x02);
    EXPECT_EQ(buf[2], 0x03);
    EXPECT_EQ(buf[3], 0x04);
  }
}

// ─────────────────────────────────────────────────────────
// 테스트 5: 에러 케이스
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, ErrorCases)
{
  // 존재하지 않는 파일 열기
  nx::file::FileStream f;
  auto ec
    = f.open(make_temp_path("nonexistent_file_xyz.bin"), nx::file::OpenMode::kRead);
  EXPECT_TRUE(ec);
  EXPECT_FALSE(f.is_open());

  // 닫힌 상태에서 write
  std::array<uint8_t, 4> dummy{};
  auto write_result = f.write(std::span<const uint8_t>(dummy));
  EXPECT_FALSE(write_result.has_value());

  // 닫힌 상태에서 read
  std::array<uint8_t, 4> buf{};
  auto read_result = f.read(buf);
  EXPECT_FALSE(read_result.has_value());

  // 닫힌 상태에서 seek
  auto seek_result = f.seek(0, nx::file::SeekOrigin::kBegin);
  EXPECT_FALSE(seek_result.has_value());

  // 닫힌 상태에서 tell
  auto tell_result = f.tell();
  EXPECT_FALSE(tell_result.has_value());

  // 닫힌 상태에서 flush
  auto flush_ec = f.flush();
  EXPECT_TRUE(flush_ec);

  // 이미 열린 파일에 재open 시도
  const auto path = temp_file("already_open.bin");
  nx::file::FileStream f2;
  ASSERT_FALSE(f2.open(path, nx::file::OpenMode::kWriteTruncate));
  auto ec2 = f2.open(path, nx::file::OpenMode::kWriteTruncate);
  EXPECT_TRUE(ec2);
}

// ─────────────────────────────────────────────────────────
// 테스트 6: 이동 연산자
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, MoveSemantics)
{
  const auto path = temp_file("move_test.bin");
  const std::vector<uint8_t> kData = {0x10, 0x20, 0x30};

  // move construct
  nx::file::FileStream original;
  ASSERT_FALSE(original.open(path, nx::file::OpenMode::kWriteTruncate));

  nx::file::FileStream moved(std::move(original));
  EXPECT_FALSE(original.is_open()); // 원본은 닫혀야 함
  EXPECT_TRUE(moved.is_open());

  // 이동된 객체로 쓰기 가능
  ASSERT_TRUE(moved.write(kData).has_value());
  moved.close();

  // move assign
  nx::file::FileStream f1;
  ASSERT_FALSE(f1.open(path, nx::file::OpenMode::kRead));

  nx::file::FileStream f2;
  f2 = std::move(f1);
  EXPECT_FALSE(f1.is_open());
  EXPECT_TRUE(f2.is_open());

  std::vector<uint8_t> buf(kData.size());
  auto result = f2.read(buf);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(buf, kData);
}

// ─────────────────────────────────────────────────────────
// 테스트 7: Flush
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, Flush)
{
  const auto path = temp_file("flush_test.bin");
  const std::vector<uint8_t> kData = {0xCA, 0xFE};

  nx::file::FileStream w;
  ASSERT_FALSE(w.open(path, nx::file::OpenMode::kWriteTruncate));
  ASSERT_TRUE(w.write(kData).has_value());

  // flush 정상 동작 확인
  auto ec = w.flush();
  EXPECT_FALSE(ec) << ec.message();
  w.close();

  // flush 후 다른 핸들로 데이터 확인
  nx::file::FileStream r;
  ASSERT_FALSE(r.open(path, nx::file::OpenMode::kRead));

  std::vector<uint8_t> buf(kData.size());
  auto result = r.read(buf);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(buf, kData);
}

// ─────────────────────────────────────────────────────────
// 테스트 8: 대용량 쓰기 (1MB+)
// ─────────────────────────────────────────────────────────

TEST_F(FileStreamTest, LargeDataWriteRead)
{
  const auto path = temp_file("large_data.bin");

  constexpr std::size_t kSize = 2 * 1024 * 1024; // 2MB
  std::vector<uint8_t> write_buf(kSize);

  // 패턴 채우기
  for (std::size_t i = 0; i < kSize; ++i) {
    write_buf[i] = static_cast<uint8_t>(i & 0xFF);
  }

  // 쓰기
  {
    nx::file::FileStream w;
    ASSERT_FALSE(w.open(path, nx::file::OpenMode::kWriteTruncate));
    auto result = w.write(write_buf);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, kSize);
  }

  // 파일 크기 확인
  {
    nx::file::FileStream f;
    ASSERT_FALSE(f.open(path, nx::file::OpenMode::kRead));
    auto size = f.file_size();
    ASSERT_TRUE(size.has_value());
    EXPECT_EQ(*size, static_cast<int64_t>(kSize));
  }

  // 읽기 후 정합성 검증
  {
    nx::file::FileStream r;
    ASSERT_FALSE(r.open(path, nx::file::OpenMode::kRead));

    std::vector<uint8_t> read_buf(kSize);
    auto result = r.read(read_buf);
    ASSERT_TRUE(result.has_value()) << result.error().message();
    EXPECT_EQ(*result, kSize);
    EXPECT_EQ(read_buf, write_buf);
  }
}

// ─────────────────────────────────────────────────────────
// main
// ─────────────────────────────────────────────────────────

int
main(int argc, char** argv)
{
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
