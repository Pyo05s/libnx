// 파일: https_client_unittest.cpp
// 생성일: 2026-02-06
// 설명: HttpsClient 단위 테스트

#include <nxnet/http/https_client.h>
#include <tests/common/coroutine_helper.h>
#include <tests/common/io_context_test_runner.h>

#include <gtest/gtest.h>
#include <nxcore/util/time_util.h>

using namespace nx::net;

// ========================================================================
// 테스트 픽스처
// ========================================================================

class HttpsClientTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    m_runner = std::make_unique<test::IoContextTestRunner>();
    m_runner->start(4); // 4개의 스레드로 시작
  }

  void TearDown() override
  {
    m_runner->stop();
    m_runner.reset();
  }

  std::unique_ptr<test::IoContextTestRunner> m_runner;
};

// ========================================================================
// 생성자 테스트
// ========================================================================

TEST_F(HttpsClientTest, Constructor_DefaultTimeout)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());
  auto opts = client->options();

  EXPECT_EQ(opts.connect_timeout.count(), 5000);
  EXPECT_EQ(opts.response_timeout.count(), 30000);
  EXPECT_TRUE(opts.ssl.verify_certificate);
}

TEST_F(HttpsClientTest, Constructor_CustomTimeout)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context(), 2000, 15000);
  auto opts = client->options();

  EXPECT_EQ(opts.connect_timeout.count(), 2000);
  EXPECT_EQ(opts.response_timeout.count(), 15000);
}

// ========================================================================
// 연결 테스트
// ========================================================================

TEST_F(HttpsClientTest, Connect_Success)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Connect_InvalidHost)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("invalid.host.test", 443);
    CO_ASSERT_TRUE(ec);
  };

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Connect_Timeout)
{
  auto client = std::make_shared<HttpsClient>(
    m_runner->io_context(), 100,
    30000); // 100ms connect timeout

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("10.255.255.1", 443);
    CO_ASSERT_TRUE(ec);
    CO_ASSERT_EQ(ec.value(), static_cast<int>(HttpErrc::connect_timeout));
  };

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Close_WithoutConnect)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->close();
    CO_ASSERT_FALSE(ec);
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// HTTPS GET 요청 테스트
// ========================================================================

TEST_F(HttpsClientTest, GetRequest_Success)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get, .target = "/get", .body = "", .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());

    const auto& response = result.value();
    CO_ASSERT_EQ(response.status_code, 200);
    CO_ASSERT_FALSE(response.body.empty());

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, GetRequest_NotFound)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/status/404",
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(result.value().status_code, 404);

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// HTTPS POST 요청 테스트
// ========================================================================

TEST_F(HttpsClientTest, PostRequest_Success)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::post,
      .target = "/post",
      .body = R"({"message": "test"})"
    };
    request.headers.set(boost::beast::http::field::content_type, "application/json");

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(result.value().status_code, 200);

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// 타임아웃 테스트
// ========================================================================

TEST_F(HttpsClientTest, ResponseTimeout)
{
  auto client = std::make_shared<HttpsClient>(
    m_runner->io_context(), 5000,
    1000); // 1초 response timeout

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/delay/3", // 3초 지연
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error().value(), static_cast<int>(HttpErrc::response_timeout));

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// 순차 요청 테스트
// ========================================================================

TEST_F(HttpsClientTest, SequentialRequests)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    for (int i = 0; i < 5; ++i) {
      HttpRequest request{
        .method = boost::beast::http::verb::get,
        .target = "/get",
        .body = "",
        .headers = {}
      };

      auto result = co_await client->send_request(request);
      CO_ASSERT_TRUE(result.has_value());
      CO_ASSERT_EQ(result.value().status_code, 200);
    }

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// SSL 검증 테스트
// ========================================================================

TEST_F(HttpsClientTest, SslVerification_Disabled)
{
  auto client = std::make_shared<HttpsClient>(m_runner->io_context());

  // SSL 검증 비활성화
  SslOptions ssl_opts{.verify_certificate = false, .ca_cert_file = "", .server_name = ""};
  client->set_ssl_options(ssl_opts);

  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get, .target = "/get", .body = "", .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_TRUE(result.has_value());
    CO_ASSERT_EQ(result.value().status_code, 200);

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

// ========================================================================
// 에러 메시지 테스트
// ========================================================================

TEST_F(HttpsClientTest, ErrorMessage_ConnectTimeout)
{
  auto ec = make_error_code(HttpErrc::connect_timeout);
  std::string message = ec.message();
  EXPECT_FALSE(message.empty());
  EXPECT_NE(message.find("timeout"), std::string::npos);
}

TEST_F(HttpsClientTest, ErrorMessage_SslHandshakeFailed)
{
  auto ec = make_error_code(HttpErrc::ssl_handshake_failed);
  std::string message = ec.message();
  EXPECT_FALSE(message.empty());
  EXPECT_NE(message.find("SSL"), std::string::npos);
}

// ========================================================================
// 리다이렉션 테스트
// ========================================================================

TEST_F(HttpsClientTest, Redirect_FollowRelativePath)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpsClient>(m_runner->io_context());

    auto ec = co_await client->connect("httpbun.com", 443);
    // auto ec = co_await client->connect("mockhttp.org", 443);
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

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Redirect_TooManyRedirects)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpsClient>(m_runner->io_context());

    auto ec = co_await client->connect("httpbun.com", 443);
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

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Redirect_DisabledOption)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpsClient>(m_runner->io_context());

    // 리다이렉션 비활성화
    HttpsClientOptions opts = client->options();
    opts.follow_redirects = false;
    client->set_options(opts);

    auto ec = co_await client->connect("httpbun.com", 443);
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

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Redirect_AbsoluteUrl)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpsClient>(m_runner->io_context());

    // 다운그레이드 허용 옵션 설정 (테스트용)
    HttpsClientOptions opts = client->options();
    opts.allow_downgrade = true;
    client->set_options(opts);

    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/absolute-redirect/1", // HTTP로 다운그레이드 시도
      .body = "",
      .headers = {}
    };

    // 다운그레이드를 허용했지만 HttpsClient는 HTTP로 전환할 수 없으므로 에러
    // 발생 예상
    auto result = co_await client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error().value(), static_cast<int>(HttpErrc::downgrade_forbidden));

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}

TEST_F(HttpsClientTest, Redirect_DowngradeBlocked)
{
  auto test_logic = [&]() -> nx::awaitable<void>
  {
    auto client = std::make_shared<HttpsClient>(m_runner->io_context());

    // 다운그레이드 금지 (기본값)
    auto ec = co_await client->connect("mockhttp.org", 443);
    CO_ASSERT_FALSE(ec);

    HttpRequest request{
      .method = boost::beast::http::verb::get,
      .target = "/absolute-redirect/1", // HTTP로 리다이렉션
      .body = "",
      .headers = {}
    };

    auto result = co_await client->send_request(request);
    CO_ASSERT_FALSE(result.has_value());
    CO_ASSERT_EQ(result.error().value(), static_cast<int>(HttpErrc::downgrade_forbidden));

    co_await client->close();
  };

  m_runner->run_sync(test_logic());
}
