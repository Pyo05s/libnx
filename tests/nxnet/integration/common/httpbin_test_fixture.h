// 파일: httpbin_test_fixture.h
// 생성일: 2026-02-10
// 설명: httpbin.org 통합 테스트 공통 픽스처

#pragma once

#include "test_config.h"
#include <nxnet/auth/auth_provider.h>
#include <nxnet/http/http_client.h>
#include <tests/common/coroutine_helper.h>
#include <tests/common/io_context_test_runner.h>

#include <boost/beast/http.hpp>
#include <gtest/gtest.h>
#include <memory>

namespace test::httpbin {

// httpbin.org 통합 테스트 기본 픽스처
class HttpbinIntegrationTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    // 멀티스레드 io_context 시작 (4 threads)
    m_runner.start(4);

    // HttpClient 생성
    m_client = std::make_unique<nx::net::HttpClient>(
      m_runner.io_context(), kConnectTimeout, kResponseTimeout);
  }

  void TearDown() override
  {
    // 클라이언트 정리
    if (m_client) {
      auto cleanup = [&]() -> nx::awaitable<void> { co_await m_client->close(); };
      try {
        m_runner.run_sync(cleanup(), nx::seconds(5));
      }
      catch (...) {
        // 정리 중 예외 무시
      }
    }

    m_client.reset();
    m_runner.stop();
  }

  // httpbin.org 연결
  nx::awaitable<std::error_code> connect_httpbin()
  {
    co_return co_await m_client->connect(kHttpbinHost, kHttpbinPort);
  }

  // 인증 헤더를 추가하고 요청 전송
  nx::awaitable_expected<nx::net::HttpResponse> send_authenticated_request(
    const std::string& target,
    nx::net::auth::AuthProvider* auth_provider,
    boost::beast::http::verb method = boost::beast::http::verb::get,
    const std::string& body = "")
  {
    // 인증 헤더 생성 (Provider가 헤더 이름과 값 모두 결정)
    nx::net::auth::AuthContext auth_ctx{
      .method = std::string(boost::beast::http::to_string(method)), .uri = target
    };

    if (!body.empty()) {
      auth_ctx.body = body;
    }

    auto headers_result = auth_provider->generate_headers(auth_ctx);
    if (!headers_result.has_value()) {
      co_return std::unexpected(headers_result.error());
    }

    // HTTP 요청 구성
    nx::net::HttpRequest request{
      .method = method, .target = target, .body = body, .headers = *headers_result
    };

    // 요청 전송
    co_return co_await m_client->send_request(request);
  }

  // 인증 없이 요청 전송 (401 Challenge 수신용)
  nx::awaitable_expected<nx::net::HttpResponse> send_unauthenticated_request(
    const std::string& target,
    boost::beast::http::verb method = boost::beast::http::verb::get)
  {
    nx::net::HttpRequest request{.method = method, .target = target};

    co_return co_await m_client->send_request(request);
  }

  test::IoContextTestRunner m_runner;
  std::unique_ptr<nx::net::HttpClient> m_client;
};

} // namespace test::httpbin
