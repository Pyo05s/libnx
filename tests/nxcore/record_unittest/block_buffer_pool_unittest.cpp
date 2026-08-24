// 파일: block_buffer_pool_unittest.cpp
// 생성일: 2026-03-30
// 설명: BlockBufferPool 및 PooledBuffer 유닛테스트

#include "nxcore/record/block_buffer_pool.h"
#include <gtest/gtest.h>
#include <thread>
#include <vector>
#include <cstring>

using namespace nx;

// 기본 풀 생성 및 버퍼 획득
TEST(BlockBufferPoolTest, AcquireReturnsBufferWithCorrectSize)
{
  auto pool = std::make_shared<record::BlockBufferPool>();
  auto buf = pool->acquire(1024);

  EXPECT_EQ(buf.size(), 1024u);
  EXPECT_GE(buf.capacity(), 1024u);
  EXPECT_FALSE(buf.empty());
  EXPECT_NE(buf.data(), nullptr);
}

// 버퍼 반환 후 재사용 확인 (힙 할당 없음)
TEST(BlockBufferPoolTest, ReleasedBufferIsReused)
{
  auto pool = std::make_shared<record::BlockBufferPool>();

  const uint8_t* first_data = nullptr;
  std::size_t first_capacity = 0;

  {
    auto buf = pool->acquire(2048);
    first_data = buf.data();
    first_capacity = buf.capacity();
    // buf 소멸 시 풀에 반환
  }

  EXPECT_EQ(pool->free_count(), 1u);

  // 두 번째 획득: 풀에서 재사용
  auto buf2 = pool->acquire(1024);

  // capacity가 유지되어야 함 (2048 이상)
  EXPECT_GE(buf2.capacity(), first_capacity);
  // 동일한 메모리 블록 재사용 확인
  EXPECT_EQ(buf2.data(), first_data);
  EXPECT_EQ(buf2.size(), 1024u);
  EXPECT_EQ(pool->free_count(), 0u);
}

// 풀이 비어있을 때 새 버퍼 할당
TEST(BlockBufferPoolTest, AcquireAllocatesWhenPoolEmpty)
{
  auto pool = std::make_shared<record::BlockBufferPool>();

  EXPECT_EQ(pool->free_count(), 0u);

  auto buf = pool->acquire(512);
  EXPECT_EQ(buf.size(), 512u);
  EXPECT_EQ(pool->free_count(), 0u);
}

// 최대 풀 크기 초과 시 버퍼 해제
TEST(BlockBufferPoolTest, ExcessBuffersAreFreed)
{
  constexpr std::size_t max_pool = 2;
  auto pool = std::make_shared<record::BlockBufferPool>(max_pool);

  // 3개 버퍼 생성 후 모두 반환
  {
    auto buf1 = pool->acquire(100);
    auto buf2 = pool->acquire(200);
    auto buf3 = pool->acquire(300);
    // 역순 소멸: buf3 → buf2 → buf1
  }

  // 최대 2개만 풀에 보관
  EXPECT_LE(pool->free_count(), max_pool);
}

// PooledBuffer 이동 생성자
TEST(PooledBufferTest, MoveConstructor)
{
  auto pool = std::make_shared<record::BlockBufferPool>();
  auto buf1 = pool->acquire(256);

  uint8_t* original_data = buf1.data();
  std::memset(buf1.data(), 0xAB, buf1.size());

  // 이동 생성
  record::PooledBuffer buf2(std::move(buf1));

  EXPECT_EQ(buf2.size(), 256u);
  EXPECT_EQ(buf2.data(), original_data);
  EXPECT_EQ(buf2.data()[0], 0xAB);

  // 원본은 비어있어야 함
  EXPECT_TRUE(buf1.empty()); // NOLINT(bugprone-use-after-move)
}

// PooledBuffer 이동 대입
TEST(PooledBufferTest, MoveAssignment)
{
  auto pool = std::make_shared<record::BlockBufferPool>();
  auto buf1 = pool->acquire(128);
  auto buf2 = pool->acquire(256);

  uint8_t* data2 = buf2.data();

  // buf1에 buf2 이동 대입: buf1의 기존 버퍼는 풀에 반환
  buf1 = std::move(buf2);

  EXPECT_EQ(buf1.size(), 256u);
  EXPECT_EQ(buf1.data(), data2);
  EXPECT_EQ(pool->free_count(), 1u); // buf1의 기존 버퍼가 반환됨
}

// 풀 없이 기본 생성된 PooledBuffer 동작 (테스트 코드 호환성)
TEST(PooledBufferTest, DefaultConstructedWorksWithoutPool)
{
  record::PooledBuffer buf;

  EXPECT_TRUE(buf.empty());
  EXPECT_EQ(buf.size(), 0u);

  // resize 정상 동작
  buf.resize(512);
  EXPECT_EQ(buf.size(), 512u);
  EXPECT_NE(buf.data(), nullptr);

  // 소멸 시 풀이 없으므로 일반 해제 (크래시 없어야 함)
}

// 적합한 capacity 버퍼 우선 선택
TEST(BlockBufferPoolTest, PreferBufferWithSufficientCapacity)
{
  auto pool = std::make_shared<record::BlockBufferPool>(4);

  // 서로 다른 크기의 버퍼 2개 생성 후 반환
  {
    auto small_buf = pool->acquire(100);
    auto large_buf = pool->acquire(10000);
    // small(100), large(10000) 순서로 반환
  }

  EXPECT_EQ(pool->free_count(), 2u);

  // 5000바이트 요청: capacity >= 5000인 large 버퍼가 선택되어야 함
  auto result = pool->acquire(5000);
  EXPECT_GE(result.capacity(), 10000u);
  EXPECT_EQ(result.size(), 5000u);

  // 하나가 사용되었으므로 1개 남음
  EXPECT_EQ(pool->free_count(), 1u);
}

// 멀티스레드 안전성 (release는 I/O 스레드에서 발생)
TEST(BlockBufferPoolTest, ThreadSafeAcquireAndRelease)
{
  auto pool = std::make_shared<record::BlockBufferPool>(8);

  constexpr int kIterations = 100;
  std::vector<std::thread> threads;

  for (int t = 0; t < 4; ++t) {
    threads.emplace_back([&pool]() {
      for (int i = 0; i < kIterations; ++i) {
        auto buf = pool->acquire(1024);
        std::memset(buf.data(), 0xFF, buf.size());
        // buf 소멸 시 풀에 반환
      }
    });
  }

  for (auto& th : threads) {
    th.join();
  }

  // 풀 상태가 유효해야 함 (크래시 없이 완료)
  EXPECT_LE(pool->free_count(), 8u);
}

// 풀 소멸 후 PooledBuffer가 안전하게 소멸 (shared_ptr로 풀 수명 연장)
TEST(BlockBufferPoolTest, PoolOutlivesBuffer)
{
  record::PooledBuffer buf;

  {
    auto pool = std::make_shared<record::BlockBufferPool>();
    buf = pool->acquire(2048);
    // pool의 shared_ptr 참조 카운트: pool + buf 내부 = 2
  }
  // pool 지역 변수 소멸, 하지만 buf가 shared_ptr을 보유

  EXPECT_EQ(buf.size(), 2048u);

  // buf 소멸 시 풀에 반환 시도 → 풀이 유일 소유자이므로 반환 후 풀도 소멸
  // 크래시 없이 완료되어야 함
}
