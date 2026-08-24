// 파일: http_client_unittest.cpp
// 생성일: 2026-02-06
// 설명: HttpClient 단위 테스트

#include <nxnet/http/http_client.h>
#include <tests/common/coroutine_helper.h>
#include <tests/common/io_context_test_runner.h>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>

using namespace nx::net;

// ========================================================================
// 테스트 픽스처
// ========================================================================

class HttpClientTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 멀티스레드 환경 (4 threads)
    m_runner.start(4);

    // 기본 타임아웃 설정 (connect: 5초, response: 30초)
    m_client = std::make_unique<HttpClient>(m_runner.io_context());
  }

  void TearDown() override
  {
    m_client.reset();
    m_runner.stop();
  }

  test::IoContextTestRunner m_runner;
  std::unique_ptr<HttpClient> m_client;
};

// ========================================================================
// 기본 기능 테스트
// ========================================================================

TEST_F(HttpClientTest, Constructor_DefaultTimeout)
{
  // 기본 타임아웃 확인
  auto options = m_client->options();
  EXPECT_EQ(options.connect_timeout.count(), 5000);
  EXPECT_EQ(options.response_timeout.count(), 30000);
}

TEST_F(HttpClientTest, Constructor_CustomTimeout)
{
  // 커스텀 타임아웃으로 생성
  auto client = std::make_unique<HttpClient>(
    m_runner.io_context(), nx::milliseconds{3000}, nx::milliseconds{15000});

  auto options = client->options();
  EXPECT_EQ(options.connect_timeout.count(), 3000);
  EXPECT_EQ(options.response_timeout.count(), 15000);
}

TEST_F(HttpClientTest, InitialState_NotConnected)
{
  EXPECT_FALSE(m_client->is_connected());
}

TEST_F(HttpClientTest, SetOptions_Success)
{
  HttpClientOptions new_options{
    .connect_timeout = nx::seconds(10), .response_timeout = nx::seconds(60)
  };

  m_client->set_options(new_options);

  auto options = m_client->options();
  EXPECT_EQ(options.connect_timeout.count(), 10000);
  EXPECT_EQ(options.response_timeout.count(), 60000);
}

// ========================================================================
// 연결 테스트 (httpbin.org 사용)
// ========================================================================

TEST_F(HttpClientTest, Connect_Success)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);
    CO_ASSERT_TRUE(m_client->is_connected());
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Connect_AlreadyConnected)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 첫 번째 연결
    auto ec1 = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec1);

    // 두 번째 연결 시도 (실패해야 함)
    auto ec2 = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_TRUE(ec2);
    CO_ASSERT_EQ(ec2, make_error_code(HttpErrc::already_connected));
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Connect_InvalidHost)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    auto ec = co_await m_client->connect("invalid.host.that.does.not.exist.com", 80);
    CO_ASSERT_TRUE(ec);
    CO_ASSERT_FALSE(m_client->is_connected());
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Close_Success)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec1 = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec1);
    CO_ASSERT_TRUE(m_client->is_connected());

    // 종료
    auto ec2 = co_await m_client->close();
    CO_ASSERT_FALSE(ec2);
    CO_ASSERT_FALSE(m_client->is_connected());
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Close_NotConnected)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결 없이 종료 (에러 없어야 함)
    auto ec = co_await m_client->close();
    CO_ASSERT_FALSE(ec);
  };

  m_runner.run_sync(test_logic());
}

// ========================================================================
// HTTP 요청 테스트
// ========================================================================

TEST_F(HttpClientTest, SendRequest_GET_Success)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    // GET 요청
    HttpRequest request{
      .method = boost::beast::http::verb::get, .target = "/get", .body = ""
    };
    request.headers.set(boost::beast::http::field::accept, "application/json");

    auto result = co_await m_client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());

    const auto& response = result.value();
    CO_ASSERT_EQ(response.status_code, 200);
    CO_ASSERT_FALSE(response.body.empty());
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(HttpClientTest, SendRequest_POST_Success)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    // POST 요청
    HttpRequest request{
      .method = boost::beast::http::verb::post,
      .target = "/post",
      .body = R"({"name": "test", "value": 123})"
    };
    request.headers.set(boost::beast::http::field::content_type, "application/json");
    request.headers.set(boost::beast::http::field::accept, "application/json");

    auto result = co_await m_client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());

    const auto& response = result.value();
    CO_ASSERT_EQ(response.status_code, 200);
    CO_ASSERT_FALSE(response.body.empty());
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

TEST_F(HttpClientTest, SendRequest_NotConnected)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결 없이 요청 (실패해야 함)
    HttpRequest request{
      .method = boost::beast::http::verb::get, .target = "/get", .body = "", .headers = {}
    };

    auto result = co_await m_client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error(), make_error_code(HttpErrc::not_connected));
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, SendRequest_404NotFound)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    // 존재하지 않는 경로로 요청
    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/this-path-does-not-exist-12345",
      .body = "",
      .headers = {}
    };

    auto result = co_await m_client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());

    const auto& response = result.value();
    CO_ASSERT_EQ(response.status_code, 404);
  };

  m_runner.run_sync(test_logic(), nx::seconds(15));
}

// ========================================================================
// 멀티스레드 안전성 테스트
// ========================================================================

TEST_F(HttpClientTest, ConcurrentRequests_Sequential)
{
  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    // 순차적으로 여러 요청 전송 (strand가 순서 보장)
    for (int i = 0; i < 5; ++i) {
      HttpRequest request{
        .method = boost::beast::http::verb::get,
        .target = "/get?id=" + std::to_string(i),
        .body = "",
        .headers = {}
      };

      auto result = co_await m_client->send_request(request);
      CO_ASSERT_TRUE(result.has_value());
      CO_ASSERT_EQ(result->status_code, 200);
    }
  };

  m_runner.run_sync(test_logic(), nx::seconds(30));
}

// ========================================================================
// 타임아웃 테스트
// ========================================================================

TEST_F(HttpClientTest, ConnectTimeout)
{
  // 짧은 타임아웃 설정
  HttpClientOptions short_timeout{
    .connect_timeout = nx::milliseconds(100), .response_timeout = nx::seconds(5)
  };
  m_client->set_options(short_timeout);

  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 응답하지 않는 IP로 연결 시도 (블랙홀)
    auto ec = co_await m_client->connect("10.255.255.1", 80);
    CO_ASSERT_TRUE(ec);
    CO_ASSERT_EQ(ec, make_error_code(HttpErrc::connect_timeout));
  };

  m_runner.run_sync(test_logic(), nx::seconds(2));
}

TEST_F(HttpClientTest, ResponseTimeout)
{
  // 짧은 response timeout 설정 (1초)
  HttpClientOptions short_timeout{
    .connect_timeout = nx::seconds(5), .response_timeout = nx::milliseconds(1000)
  };
  m_client->set_options(short_timeout);

  auto test_logic = [this]() -> nx::awaitable<void>
  {
    // 연결
    auto ec = co_await m_client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    // 3초 지연 요청 (timeout 발생해야 함)
    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/delay/3",
      .body = "",
      .headers = {}
    };

    auto result = co_await m_client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error(), make_error_code(HttpErrc::response_timeout));
  };

  m_runner.run_sync(test_logic(), nx::seconds(5));
}

// ========================================================================
// 에러 처리 테스트
// ========================================================================

TEST_F(HttpClientTest, ErrorCode_Messages)
{
  auto ec1 = make_error_code(HttpErrc::connection_failed);
  EXPECT_EQ(std::string(ec1.message()), "Connection failed");

  auto ec2 = make_error_code(HttpErrc::connect_timeout);
  EXPECT_EQ(std::string(ec2.message()), "Connection timeout");

  auto ec3 = make_error_code(HttpErrc::not_connected);
  EXPECT_EQ(std::string(ec3.message()), "Not connected");
}

// ========================================================================
// 리다이렉션 테스트
// ========================================================================

TEST_F(HttpClientTest, Redirect_FollowRelativePath)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpClient>(m_runner.io_context());

    auto ec = co_await client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/redirect/2", // 2회 리다이렉션
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(result.value().status_code, 200);

    co_await client->close();
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Redirect_TooManyRedirects)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpClient>(m_runner.io_context());

    auto ec = co_await client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/redirect/10", // 10회 리다이렉션 (기본 max는 5)
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error().value(), static_cast<int>(HttpErrc::too_many_redirects));

    co_await client->close();
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Redirect_DisabledOption)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpClient>(m_runner.io_context());

    // 리다이렉션 비활성화
    HttpClientOptions opts = client->options();
    opts.follow_redirects = false;
    client->set_options(opts);

    auto ec = co_await client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/redirect/1",
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    // 리다이렉션을 따라가지 않으므로 302 응답을 받음
    CO_ASSERT_TRUE(
      result.value().status_code == 301 || result.value().status_code == 302);

    co_await client->close();
  };

  m_runner.run_sync(test_logic());
}

TEST_F(HttpClientTest, Redirect_AbsoluteUrl)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpClient>(m_runner.io_context());

    auto ec = co_await client->connect("127.0.0.1", 80);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/absolute-redirect/2", // 절대 URL 리다이렉션
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(result.value().status_code, 200);

    co_await client->close();
  };

  m_runner.run_sync(test_logic());
}
