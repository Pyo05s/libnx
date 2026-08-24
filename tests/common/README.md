# 테스트 공통 유틸리티 가이드

## IoContextTestRunner

비동기/코루틴 기반 테스트를 위한 필수 유틸리티입니다.

### 사용이 필수인 경우

- `nx::awaitable<T>` 반환 함수 테스트
- `co_await` 키워드 사용하는 코드 테스트
- `boost::asio::co_spawn()` 호출하는 코드 테스트
- 비동기 io_context 작업이 포함된 테스트

### 사용하지 않을 경우 발생하는 문제

```cpp
// ❌ 잘못된 예 - 여러 문제 발생
TEST_F(MyTest, BadExample) {
    AsioContext ioc;

    // 문제 1: 코루틴이 실행되기 전에 ioc 소멸
    boost::asio::co_spawn(ioc, my_async_func(), boost::asio::detached);
    ioc.run();  // 여기서 실행되지만...

    // 문제 2: 테스트 종료 시 리소스 누수
    // 문제 3: stop() 없이 스레드 무한 대기
    // 문제 4: 테스트 실패 시 정리 안됨
}
```

### 올바른 사용법

```cpp
// ✅ 올바른 예
class MyTest : public ::testing::Test {
protected:
    test::IoContextTestRunner m_runner;
};

TEST_F(MyTest, GoodExample) {
    auto test_logic = [&]() -> nx::awaitable<void> {
        // 모든 비동기 작업을 코루틴 내부에서
        auto result = co_await my_async_func();
        EXPECT_EQ(result, expected);

        // 명시적 정리
        co_await cleanup();
    };

    // run_sync가 완료를 보장
    m_runner.run_sync(test_logic());
}
```

### 주요 기능

#### 1. run_sync() - 동기적 코루틴 실행

```cpp
// void 반환
m_runner.run_sync([&]() -> nx::awaitable<void> {
    co_await async_operation();
});

// 값 반환
auto result = m_runner.run_sync([&]() -> nx::awaitable<int> {
    co_return co_await get_value();
});

// 타임아웃 지정
m_runner.run_sync(test_logic(), nx::seconds(30));
```

#### 2. 백그라운드 실행 (장시간 서버 테스트)

```cpp
TEST_F(ServerTest, LongRunning) {
    m_runner.start_background();  // io_context 백그라운드 실행

    // 서버 시작
    co_spawn(m_runner.io_context(), server->start(), detached);

    // 클라이언트 요청들...

    m_runner.stop();  // 자동 정리
}
```

### Fixture 통합 예제

```cpp
class RecorderSessionTest : public ::testing::Test {
protected:
    void SetUp() override {
        m_segment_manager = create_segment_manager(m_runner.io_context());
        m_session_manager = create_session_manager(m_runner.io_context());
    }

    void TearDown() override {
        // 명시적 정리 (run_sync 사용)
        if (m_session_manager) {
            m_runner.run_sync([&]() -> nx::awaitable<void> {
                co_await m_session_manager->stop_all_sessions();
            });
        }
        // m_runner 소멸자가 io_context 정리
    }

protected:
    test::IoContextTestRunner m_runner;
    std::unique_ptr<SessionManager> m_session_manager;
};
```

## CoroutineTestHelper

간단한 비동기 대기가 필요한 경우 사용합니다.

```cpp
TEST(SimpleTest, WaitForCondition) {
    std::atomic<bool> ready{false};

    std::thread worker([&]() {
        std::this_thread::sleep_for(100ms);
        ready = true;
    });

    // 조건 대기
    bool success = test::wait_for_condition(ready, nx::seconds(5));
    ASSERT_TRUE(success);

    worker.join();
}
```

## 참고 예제 코드

- `tests/common/coroutine_test_template.cpp.template` - 복사해서 사용하는 테스트 템플릿
- `tests/apps/record_server/recorder/record_db_buffer_unittest.cpp` - IoContextTestRunner 사용
- `tests/nxcore/record_unittest/segment_builder_*_unittest.cpp` - 다양한 비동기 테스트 패턴

## 새 테스트 파일 시작하기

```powershell
# 템플릿 복사
Copy-Item tests/common/coroutine_test_template.cpp.template tests/my_project/my_test.cpp
# 내용 수정 후 사용
```

## 체크리스트

테스트 작성 전 확인:

- [ ] 코루틴 사용? → IoContextTestRunner 필수
- [ ] co_spawn() 사용? → IoContextTestRunner 필수
- [ ] 직접 io_context 생성? → 금지, IoContextTestRunner 사용
- [ ] 수동 스레드 관리? → 금지, IoContextTestRunner 사용
- [ ] TearDown에서 비동기 정리? → run_sync() 사용
