# 코딩 규칙 및 저장소 가이드라인

이 지침 가이드는 저장소 내에서 일관되고 고품질의 최신 C++20/23 코드를 작성하기 위한 규칙과 모범 사례를 제공합니다. 모든 기여자는 이 가이드를 준수하여 코드의 가독성, 유지보수성 및 성능을 보장해야 합니다.

※ 필수 원칙

- 모든 코드 변경/추가 코드는 C++로 작성해야 합니다.
- 모든 소스 파일 내 주석은 한국어로 작성하세요.
- AI 에이전트와의 모든 대화 및 응답은 한국어로 합니다.
  - 사용자는 이 저장소 작업 중 답변을 한국어로 받기를 원합니다.
- 이 저장소에서 사용하는 모든 한글 텍스트(주석, 문서 등)는 UTF-8 인코딩을 사용하도록 권장합니다.

## Agent 일반 지침

- `.github/agents-instructions.md` 파일을 읽고 반드시 실천 합니다.
- 대화 중에 절대 자동으로 코드 수정, 구현하지 않습니다. 해당 상황이 발생하면 반드시 사용자에게 알리고, 사용자의 명시적인 요청이 있을 때만 코드를 제안하거나 수정합니다.
- 코드 확인 중 발견한 문제점이나 개선 사항이 있다면, 반드시 사용자에게 알려야 합니다. 사용자가 요청하지 않은 경우에도 문제점이나 개선 사항을 숨기지 말고 투명하게 공유해야 합니다.

## Web 개발 일반 지침

- record_server api 를 사용하거나 추가 개발 시 `apps/record_server/docs/API_REFERENCE.md` 파일을 참조하여 API 명세를 작성하고, 문서와 구현이 다른 경우 사용자에게 알려 항상 문서를 최신화 하도록 합니다.

## MCP 사용 지침

[Crucial Rule for ripgrep]
When searching with the 'ripgrep' tool, if the pattern contains a pipe character (|) or spaces, you MUST escape it or wrap the entire pattern argument with literal double quotes inside the JSON payload, like "\"A|B\"". Do not pass raw pipe characters to the shell.

## ✅ C++ 일반 지침

- 현재 저장소의 c++ 표준은 C++23입니다. 모든 코드는 C++23 표준을 준수해야 합니다.
- 모든 새 파일은 최상단에 '첫 문단' 형태로 파일명과 생성일을 기록해야 합니다

```cpp
// 파일: nxrsrv.cpp
// 생성일: 2025-11-03
// 설명: 짧은 파일 목적 설명(한 문장)
```

- 빌드하기 전에 CMakePresets.json 을 사용하여 프로젝트 설정을 확인하세요.

## ✅ 명명 규칙

일관된 명명 규칙은 코드 가독성의 핵심입니다. Microsoft의 가이드라인을 따르는 것을 권장합니다.

| 요소                | 명명 규칙                           | 예시                                                                    |
| ------------------- | ----------------------------------- | ----------------------------------------------------------------------- |
| namespace           | snake_case                          | `namespace family`, `namespace people_count`                            |
| 클래스, 타입        | PascalCase                          | `class User`, `class UserAccount`                                       |
| 함수                | snake_case                          | `int get_user_count() const`, `std::string get_user_name(int id) const` |
| 매개변수, 지역 변수 | snake_case                          | `int user_count`, `string customer_name`                                |
| 멤버 변수           | m밑줄(m\_) + snake_case             | `std::string m_connectionString;`                                       |
| 상수 및 열거형      | k(소문자)파스칼 케이스 (PascalCase) | `static const int kDefaultTimeout = 5000;`                              |

## ✅ 코드 서식 및 가독성 (Formatting & Readability)

일관된 서식은 코드를 시각적으로 파싱하기 쉽게 만듭니다.
C++ formatting 은 vscode 에 설정된 .clang-format 파일을 따르도록 권장합니다. (저장소 루트에 위치)

## ✅ 비동기 코드 안전성 규칙

- `co_spawn`, 코루틴 람다 등 **비동기 경계를 넘는 람다에서 raw `this` 캡처를 금지합니다.**
  - 클래스가 `std::enable_shared_from_this<T>`를 상속하고 `make_shared`로 생성된 경우에만 `shared_from_this()`를 통해 안전하게 캡처할 수 있습니다.
  - 올바른 패턴: `[self = shared_from_this()]() -> nx::awaitable<void> { co_await self->foo(); }`
  - 금지 패턴: `[this]() -> nx::awaitable<void> { co_await foo(); }` — co_spawn에 전달할 경우 수명 위반 가능
- 비동기 경계를 넘는 람다를 포함하는 클래스는 반드시 `std::enable_shared_from_this<T>`를 상속하고 `make_shared`로 생성되어야 합니다.
- 모듈의 `stop()` 완료 후 해당 모듈을 소유하는 `shared_ptr`를 `reset()`하여 수명을 명시적으로 종료해야 합니다.

## ✅ Moedern C++20/23 코드 작성 규칙

- 가독성 향상에 도움이 될 경우 **auto**를 사용하여 타입 추론 수행
- 원시 포인터 대신 **smart pointers**(`std::unique_ptr`, `std::shared_ptr`) 활용
- 기존 이터레이터 루프 대신 **range-based for loops** 적용
- 컴파일 시 상수 및 함수에는 **constexpr** 사용
- tuple/pair unpacking 에는 **구조적 바인딩** 활용
- 선택적 값에는 **null 포인터** 대신 **std::optional** 사용
- 적용 가능한 경우 템플릿 제약 조건에 **concepts** 적용
- 안전한 배열/컨테이너 뷰에는 **std::span** 사용
- 읽기 전용 문자열 매개변수에 **std::string_view** 활용
- 구조체 초기화에 **designated initializers** 적용
- 반환값에 대한 오류 처리가 필요한 함수에는 **std::expected** 활용
- 비동기 작업에는 **코루틴**과 **boost::asio** 사용

### `[[nodiscard]]` 적용 기준

**핵심 판단 원칙**: "반환값을 무시하는 것이 **거의 확실히 버그**인가?"

**적용 대상 (MUST)**:
| 케이스 | 예시 | 비고 |
|--------|------|------|
| `awaitable<std::error_code>` 등 비동기 에러 반환 | 에러 무시 방지, co\*await 없이 호출 금지 ||
| 리소스 할당 된 포인터 또는 `unique_ptr` 반환 | `create()` 등 메모리 누수 방지, 소유권 이전 명확화 | `shared_ptr` 등 메모리 해제가 필요없는 대상 반환은 대상이 아님 |
| 리소스 소유권 이전 (팩토리/acquire) | `unique_ptr` 반환, `acquire()` 등 — 누수 방지 | `shared_ptr` 등 메모리 해제가 필요없는 대상 반환은 대상이 아님 |
| 부작용이 있으면서 결과도 필수인 함수 | `next_cseq()` (시퀀스 증가 + 값 사용 필수) ||
| 호출자가 혼동할 수 있는 함수 |`empty()`vs`clear()` 유형 ||

**적용하지 않을 대상 (DO NOT)**:
| 케이스 | 예시 |
|--------|------|
| 단순 getter/accessor | `is_running()`, `session_id()`, `mode()` |
| const 참조/값 반환 속성 조회 | `tracks()`, `output_url()`, `state()` |
| 통계/모니터링 조회 | `frames_processed()`, `bytes_processed()` |
| 상태 확인 bool 함수 | `is_connected()`, `is_healthy()`, `has_video()` |
| `std::error_category` 오버라이드 | `name()`, `message()` — 표준이 미적용 |

## ✅ 프로젝트 구조 및 파일 구성

- 다음 구조에 따라 프로젝트 파일을 구성하세요:
  - `apps/` - 배포 가능한 애플리케이션 소스 코드
  - `nxcore/` - 내부 라이브러리 및 유틸리티
  - `tests/` - 라이브러리 빛 애플리케이션 테스트 코드
  - `scripts/` - 유용한 스크립트 모음
  - `docs/` - 문서

- 헤더 파일 조건:
  - include guards 또는 `#pragma once` 를 사용하여 중복 포함 방지
  - 가능한 경우 클래스 전방 선언(Forward declare) 사용
  - public, protected, private 섹션을 명확히 구분
  - 내부 라이브러리 헤더는 표준 라이브러리 헤더보다 먼저 포함

- namespace:
  - 절대로 `using namespace` 지시문을 사용하지 않습니다.
  - 테스트를 제외한 모든 코드는 `nx` 네임스페이스 내에 작성
  - 어플리케이션은 프로젝트 별로 하위 네임스페이스 사용 가능
  - 내부 라이브러리는 종류별로 하위 네임스페이스 사용 가능
  - cpp 내부 로컬에서만 사용하는 헬퍼 함수/객체/변수 등은 익명 네임스페이스(`namespace { ... }`) 안에 정의하여 외부 노출을 방지
  - 소스 파일 내에서는 반드시 `fully qualified names` 사용
  - 헤더 파일 내에서는 `partial qualified names` 사용 가능

## ✅ 내부 유틸 활용

nxcore 내부 유틸리티를 적극 활용하여 코드 일관성과 재사용성을 높이세요.
특히, 시간 관련 함수는 반드시 `nxcore/util/time_util.h`의 유틸리티를 사용해야 합니다.

- 만약 구현이 안되어 있는 기능이 있다면, 추가 요청해야 합니다.

| 종류      | 설명                                | 위치                                       |
| --------- | ----------------------------------- | ------------------------------------------ |
| 디버깅    | 조건 검증, 디버그 출력 등           | `repository_root`/nxcore/util/debug_util.h |
| 타입 정의 | 복사 및 이동 금지, 스마트 포인터 등 | `repository_root`/nxcore/util/type_util.h  |
| 시간 관련 | 시간 제어, timestamp 생성 등        | `repository_root`/nxcore/util/time_util.h  |

## ✅ 성능 및 메모리 관리

- 비용이 많이 드는 리소스에 **lazy initialization**를 적용하세요
- 처리할 수 있는 구체적인 예외만 catch 하세요. catch (...)와 같이 일반적인 예외를 잡는 것은 피해야 합니다.
- 예외는 프로그램 흐름 제어를 위해 사용하지 마세요. 예외는 예상치 못한 오류 상황에만 사용되어야 합니다.
- 예외를 대신 할 수 있는 인터페이스를 제공하는 경우, 예외 처리 대신 사용 합니다.

```cpp
	// 예외 처리가 필요한
	// std::filesystem::create_directories(dir);
	// 대신
	std::error_code ec;
	std::filesystem::create_directories(dir, ec);
	if (ec)
		return ec;
```

- 불필요한 객체 할당을 피하고, 특히 루프 내에서는 주의하세요.
- 대용량 데이터 처리를 위해 **move semantics**를 적극 활용하세요.
- 반복문 내에서 불필요한 계산을 피하기 위해 **캐싱**을 사용하세요.
- 메모리 누수를 방지하기 위해 **RAII(Resource Acquisition Is Initialization)** 원칙을 준수하세요.
- 성능이 중요한 코드 경로에서는 **프로파일링 도구**를 사용하여 병목 현상을 식별하고 최적화하세요.
- 함수에는 가능한 출력 매개변수를 사용하지 말고 반환값을 사용하세요.

## ✅ 보안 (Security) 및 개인정보 보호(Privacy)

안전한 코드를 작성하기 위한 기본 원칙입니다.

| 보안 영역          | 규칙                   | 설명                                                                                                                                                |
| ------------------ | ---------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------- |
| 입력 유효성 검사   | 모든 외부 데이터 검증  | 외부(사용자, API 등)로부터 들어오는 모든 데이터는 신뢰하지 않고 항상 유효성을 검사하세요.                                                           |
| SQL 삽입 방지      | 매개변수화된 쿼리 사용 | 항상 매개변수화된 쿼리나 Entity Framework와 같은 ORM을 사용하여 SQL 삽입 공격을 방지하세요.                                                         |
| 민감한 데이터 보호 | 구성 관리 도구 사용    | 비밀번호, 연결 문자열, API 키 등은 소스 코드에 하드코딩하지 말고 Secret Manager, Azure Key Vault 등을 사용하세요.                                   |
| 익명화             | 민감한 데이터 익명화   | 로그 파일이나 디버깅 출력에 민감한 정보(예: 사용자 이름, 이메일, IP 주소 등)가 포함되지 않도록 익명화하고, 필요한 경우 적절한 익명화를 해야 합니다. |
| 권한               | 시스템 권한 확인       | 시스템 수준 작업에 적절한 권한 확인 적용                                                                                                            |
| 파일               | 안전한 파일 위치 처리  | 안전한 파일 위치 처리를 위해 가능한 표준 라이브러리를 사용하세요.                                                                                   |

## ✅ 테스트 및 품질 보증

- 모든 소스 파일에는 대응되는 테스트 파일이 있어야 합니다.
- 단위 테스트에는 **Google 테스트 프레임워크** 사용
- 비동기 작업 테스트에는 `tests/common` 디렉터리의 헬퍼 클래스 활용
  - 코루틴 테스트를 위한 **CoroutineTestHelper**
  - Boost Asio IO Context 테스트를 위한 **IoContextRunner**
- 외부 종속성을 위한 **mock objects** 구현
- 데이터 주도 테스트를 위해 **TEST_P** 적용
- 성능 테스트를 위해 `Google Benchmark`를 활용한 **benchmarking** 사용
- 모든 신규 기능이 테스트되도록 `tests` 디렉터리에 포함된 테스트 프레임워크 활용
- 플랫폼 독립적 경로 처리
  - 경로 조작 시 `std::filesystem` API 활용
  - 경로 비교 시 `generic_string()` 활용
  - 하드코딩된 절대 경로 사용 금지
- 임시 테스트 데이터는 `tests/temp/` 디렉터리에 저장 하고 테스트 후 정리
- 테스트 용 미디어 파일은 'tests/data/media/' 디렉터리에 있습니다. 미디어 파일이 필요 할 경우 사용 할 수 있습니다.

### 비동기/코루틴 테스트 규칙

**필수 사항:**

- **모든 코루틴 기반 테스트는 `test::IoContextTestRunner`를 사용해야 합니다**
- 직접 `io_context`를 생성하거나 `co_spawn()` + `detached` 사용 금지
- 수동 스레드 관리 금지 (IoContextTestRunner가 자동 관리)
- test logic 을 위한 함수 안에서는 `tests/common/coroutine_helper.h`의 `CO_ASSERT_*` 등의 매크로 사용해야 합니다.

**IoContextTestRunner 사용 이유:**

1. 코루틴 수명 주기가 io_context보다 길어지는 문제 방지
2. 테스트 종료 시 리소스 자동 정리 보장
3. 타임아웃 지원으로 무한 대기 방지
4. 테스트 간 io_context 상태 격리

**표준 패턴:**

```cpp
TEST_F(MyTest, CoroTest) {
    auto test_logic = [&]() -> nx::awaitable<void> {
        // 1. 비동기 작업 수행
        co_await async_operation();

        // 2. 결과 검증
        EXPECT_TRUE(condition);

        // 3. 리소스 정리 (명시적 co_await)
        co_await cleanup_async_resources();
    };

    // run_sync가 순차 실행 보장
    m_runner.run_sync(test_logic());
}
```

**참고 예제:**

- `tests/recorder_unittest/record_db_buffer_unittest.cpp`
- `tests/common/io_context_test_runner.h`

## ✅ 외부 종속성 관리

### Third-Party 연동 계정

공용 테스트용, 개인 테스트는 별도로 제공 가능

| 이름    | 설명       | 계정 정보                                     |
| ------- | ---------- | --------------------------------------------- |
| MariaDB | 기본 RDBMS | IP:127.0.0.1, Port: 3306, ID: test, PW: test1 |

## ✅ 코드 품질 및 유지보수성

- **SOLID 원칙** 준수:
  - Single Responsibility: 각 클래스는 하나의 명확한 목적을 가짐
  - Open/Closed: 확장을 위해 플러그인 시스템 사용(DLL 활용)
  - Liskov Substitution: 적절한 상속 계층 구조, 상속보다는 합성(Composition)을 우선적으로 고려하십시오.
  - Interface Segregation: 거대한 인터페이스 하나보다 작고 구체적인 여러 개의 인터페이스가 낫습니다.
    - C++20의 `Concepts`를 활용하면 템플릿 코드에서 요구 사항을 명확히 정의하여 ISP를 자연스럽게 구현할 수 있습니다.
  - Dependency Inversion: 구체적인 클래스 대신 추상 클래스(순수 가상 함수 포함)를 주입받으십시오.
    - **std::unique_ptr**나 **std::shared_ptr**를 사용한 의존성 주입(Dependency Injection) 패턴을 적용하여 객체의 수명 주기를 안전하게 관리하십시오.

## ✅ 문서화 규칙

- 모든 문서는 자동으로 생성하지 않습니다.

## ✅ 추가 Copilot 동작 설정

- 조기 최적화보다 **가독성과 유지보수성**을 우선시합니다.
- 새로운 C++20/23 기능을 적극 활용하여 현대적인 코드를 작성합니다.
- 어플리케이션 구조에 기반한 적절한 클래스 배치를 제안합니다.
- 멀티스레드 작업 시 **스레드 안전성**을 고려한 코드를 작성합니다.
- 시스템 레벨 코드 제안 시 **플랫폼 차이**를 고려합니다.
- 코드 주석은 간결하고 명확하게 작성하며, 불필요한 장황함을 피하십시오.
- 답변을 "네", "맞아요!" 등으로 시작하지 말고 간결하고 명확하게 표현하십시오.
- 사용자에게 노출되는 텍스트 작성 시 불필요한 감탄사나 비공식적 표현 없이 **명확하고 전문적인 언어**를 사용하십시오.
- 제목과 표제는 문장체(Sentence case)를 적용하십시오.
- 실시간 스트리밍/녹화 등 연속적인 작업에서 발생하는 오류는 최대한 사용자(또는 클라이언트)에게 투명하게 알리고, 여러 단계에 걸쳐 오류를 처리하더라도 안정적으로 복구 할 수 있는 방식을 고민해야 합니다. 문제가 없는 것 처럼보일 수 있는 오류 복구 속도가 중요한 것이 아닙니다.
- 문제가 발생하면 단순 방어 코드를 추가하는 방식보다는, 문제의 근본 원인을 먼저 분석하여 절차적으로 문제를 해결하는 방식을 선호합니다.
