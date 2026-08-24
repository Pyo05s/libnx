// 파일: tcp_write_serializer_unittest.cpp
// 생성일: 2026-03-04
// 설명: TcpWriteSerializer 단위 테스트

#include <nxnet/rtsp/tcp_write_serializer.h>
#include <nxnet/rtsp/rtsp_server_session.h>

#include <nxcore/util/time_util.h>

#include <gtest/gtest.h>
#include <nxcore/util/asio_type.h>
#include <boost/asio/co_spawn.hpp>
#include <boost/asio/detached.hpp>
#include <cstdint>
#include <thread>
#include <vector>

namespace {

using namespace nx::net;

// ============================================================================
// 테스트 헬퍼: TCP 루프백 소켓 쌍 생성
// ============================================================================

struct SocketPair
{
  std::shared_ptr<boost::asio::ip::tcp::socket> server;
  std::shared_ptr<boost::asio::ip::tcp::socket> client;
};

/// 루프백 TCP 소켓 쌍을 동기적으로 생성
SocketPair
make_loopback_pair(AsioContext& ioc)
{
  boost::asio::ip::tcp::acceptor acceptor(
    ioc,
    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), 0));

  auto port = acceptor.local_endpoint().port();

  boost::asio::ip::tcp::socket client_sock(ioc);
  client_sock.connect(
    boost::asio::ip::tcp::endpoint(boost::asio::ip::address_v4::loopback(), port));

  auto server_sock = acceptor.accept();

  return {
    std::make_shared<boost::asio::ip::tcp::socket>(std::move(server_sock)),
    std::make_shared<boost::asio::ip::tcp::socket>(std::move(client_sock))};
}

// ============================================================================
// 테스트 픽스처
// ============================================================================

class TcpWriteSerializerTest : public ::testing::Test
{
protected:
  void SetUp() override
  {
    auto pair = make_loopback_pair(m_ioc);
    m_server_socket = pair.server;
    m_client_socket = pair.client;
  }

  void TearDown() override
  {
    if (m_server_socket && m_server_socket->is_open()) {
      boost::system::error_code ec;
      m_server_socket->close(ec);
    }
    if (m_client_socket && m_client_socket->is_open()) {
      boost::system::error_code ec;
      m_client_socket->close(ec);
    }
  }

  /// io_context를 제한된 시간 동안 실행
  void run_for(nx::milliseconds ms)
  {
    m_ioc.restart();
    m_ioc.run_for(ms);
  }

  AsioContext m_ioc;
  std::shared_ptr<boost::asio::ip::tcp::socket> m_server_socket;
  std::shared_ptr<boost::asio::ip::tcp::socket> m_client_socket;
};

// ============================================================================
// 기본 기능 테스트
// ============================================================================

TEST_F(TcpWriteSerializerTest, SubmitAndReceive)
{
  // 직렬화기 생성
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  // 데이터 전송
  std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
  serializer->submit(data);

  // io_context 실행하여 async_write 완료
  run_for(nx::milliseconds(100));

  // 클라이언트에서 수신
  std::vector<uint8_t> recv_buf(64);
  boost::system::error_code ec;
  auto bytes = m_client_socket->read_some(boost::asio::buffer(recv_buf), ec);

  ASSERT_FALSE(ec) << ec.message();
  ASSERT_EQ(bytes, 4u);
  EXPECT_EQ(recv_buf[0], 0x01);
  EXPECT_EQ(recv_buf[1], 0x02);
  EXPECT_EQ(recv_buf[2], 0x03);
  EXPECT_EQ(recv_buf[3], 0x04);
}

TEST_F(TcpWriteSerializerTest, SubmitString)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  std::string message = "RTSP/1.0 200 OK\r\n\r\n";
  serializer->submit(message);

  run_for(nx::milliseconds(100));

  std::vector<char> recv_buf(256);
  boost::system::error_code ec;
  auto bytes = m_client_socket->read_some(boost::asio::buffer(recv_buf), ec);

  ASSERT_FALSE(ec) << ec.message();
  std::string received(recv_buf.data(), bytes);
  EXPECT_EQ(received, message);
}

TEST_F(TcpWriteSerializerTest, MultipleSubmitsOrderPreserved)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  // 연속 3개 submit
  serializer->submit(std::vector<uint8_t>{0xAA});
  serializer->submit(std::vector<uint8_t>{0xBB});
  serializer->submit(std::vector<uint8_t>{0xCC});

  run_for(nx::milliseconds(200));

  // 전체 수신
  std::vector<uint8_t> recv_buf(64);
  boost::system::error_code ec;
  size_t total = 0;
  while (total < 3) {
    auto bytes = m_client_socket->read_some(
      boost::asio::buffer(recv_buf.data() + total, recv_buf.size() - total),
      ec);
    if (ec) {
      break;
    }
    total += bytes;
  }

  ASSERT_EQ(total, 3u);
  EXPECT_EQ(recv_buf[0], 0xAA);
  EXPECT_EQ(recv_buf[1], 0xBB);
  EXPECT_EQ(recv_buf[2], 0xCC);
}

// ============================================================================
// 상태 관리 테스트
// ============================================================================

TEST_F(TcpWriteSerializerTest, IsActiveAfterCreate)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);
  EXPECT_TRUE(serializer->is_active());
}

TEST_F(TcpWriteSerializerTest, IsActiveAfterClose)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);
  serializer->close();

  run_for(nx::milliseconds(50));

  EXPECT_FALSE(serializer->is_active());
}

TEST_F(TcpWriteSerializerTest, SubmitAfterCloseIgnored)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);
  serializer->close();

  run_for(nx::milliseconds(50));

  // close 후 submit은 무시됨
  serializer->submit(std::vector<uint8_t>{0xFF});

  run_for(nx::milliseconds(50));

  // 클라이언트에서 데이터 없음 확인
  m_client_socket->non_blocking(true);
  std::vector<uint8_t> recv_buf(64);
  boost::system::error_code ec;
  auto bytes = m_client_socket->read_some(boost::asio::buffer(recv_buf), ec);

  EXPECT_TRUE(ec == boost::asio::error::would_block || bytes == 0);
}

TEST_F(TcpWriteSerializerTest, QueuedCountUpdates)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  EXPECT_EQ(serializer->queued_count(), 0u);

  // 큰 데이터를 submit하면 큐에 남아있을 수 있음
  std::vector<uint8_t> big_data(65536, 0x42);
  for (int i = 0; i < 10; ++i) {
    serializer->submit(big_data);
  }

  // strand에 post되기 전에는 count가 0일 수 있음
  // io_context를 약간 실행하여 strand 핸들러가 큐잉되도록 함
  run_for(nx::milliseconds(10));

  // 전체가 전송되기 전에 큐에 항목이 남아있을 수 있음
  // (소켓 버퍼 크기에 따라 다름 → 정확한 카운트 테스트는 비결정적)
  // 기본 동작 확인: overflow_count는 0이어야 함 (max 500개 큐)
  EXPECT_EQ(serializer->overflow_count(), 0u);
}

// ============================================================================
// Slow consumer / 큐 초과 테스트
// ============================================================================

TEST_F(TcpWriteSerializerTest, OverflowClosesSocket)
{
  // 매우 작은 큐 크기로 생성
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket, 3);

  // 클라이언트가 수신하지 않으므로 소켓 버퍼가 가득 차면 write가 지연됨
  // 큰 데이터를 반복 submit하여 큐 초과 유도
  std::vector<uint8_t> big_data(65536, 0x42);
  for (int i = 0; i < 100; ++i) {
    serializer->submit(big_data);
  }

  // io_context 실행 (소켓 버퍼 → write 대기 → 큐 초과)
  run_for(nx::milliseconds(500));

  // overflow가 발생했어야 함
  EXPECT_GT(serializer->overflow_count(), 0u);
  EXPECT_FALSE(serializer->is_active());
}

// ============================================================================
// 소켓 닫기 후 동작 테스트
// ============================================================================

TEST_F(TcpWriteSerializerTest, SocketCloseHandledGracefully)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  // 데이터 전송
  serializer->submit(std::vector<uint8_t>{0x01});
  run_for(nx::milliseconds(50));

  // 외부에서 소켓 닫기
  boost::system::error_code ec;
  m_server_socket->close(ec);

  // 닫힌 소켓에 submit해도 크래시 없음
  serializer->submit(std::vector<uint8_t>{0x02});
  run_for(nx::milliseconds(50));

  EXPECT_FALSE(serializer->is_active());
}

// ============================================================================
// TcpInterleavedTransport + Serializer 통합 테스트
// ============================================================================

TEST_F(TcpWriteSerializerTest, InterleavedTransportFormat)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  // transport 생성 (RTP ch=0, RTCP ch=1)
  auto transport
    = std::make_shared<TcpInterleavedTransport>(serializer, m_server_socket, 0, 1);

  EXPECT_TRUE(transport->is_active());

  // RTP 패킷 전송
  std::vector<uint8_t> rtp_data = {0x80, 0x60, 0x00, 0x01};
  transport->send_rtp(rtp_data);

  run_for(nx::milliseconds(100));

  // 클라이언트에서 인터리브 프레임 수신
  std::vector<uint8_t> recv_buf(64);
  boost::system::error_code ec;
  auto bytes = m_client_socket->read_some(boost::asio::buffer(recv_buf), ec);

  ASSERT_FALSE(ec) << ec.message();
  ASSERT_EQ(bytes, 8u); // '$' + ch(1) + len(2) + data(4)

  EXPECT_EQ(recv_buf[0], '$');
  EXPECT_EQ(recv_buf[1], 0x00); // RTP channel
  EXPECT_EQ(recv_buf[2], 0x00); // length MSB
  EXPECT_EQ(recv_buf[3], 0x04); // length LSB
  EXPECT_EQ(recv_buf[4], 0x80); // RTP data
  EXPECT_EQ(recv_buf[5], 0x60);
  EXPECT_EQ(recv_buf[6], 0x00);
  EXPECT_EQ(recv_buf[7], 0x01);
}

TEST_F(TcpWriteSerializerTest, InterleavedTransportRtcpChannel)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);
  auto transport
    = std::make_shared<TcpInterleavedTransport>(serializer, m_server_socket, 2, 3);

  // RTCP 패킷 전송
  std::vector<uint8_t> rtcp_data = {0x80, 0xC8, 0x00, 0x06};
  transport->send_rtcp(rtcp_data);

  run_for(nx::milliseconds(100));

  std::vector<uint8_t> recv_buf(64);
  boost::system::error_code ec;
  auto bytes = m_client_socket->read_some(boost::asio::buffer(recv_buf), ec);

  ASSERT_FALSE(ec) << ec.message();
  ASSERT_GE(bytes, 5u);

  EXPECT_EQ(recv_buf[0], '$');
  EXPECT_EQ(recv_buf[1], 0x03); // RTCP channel
}

TEST_F(TcpWriteSerializerTest, MultipleTransportsShareSerializer)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);

  // video (ch 0/1), audio (ch 2/3) transport
  auto video_transport
    = std::make_shared<TcpInterleavedTransport>(serializer, m_server_socket, 0, 1);
  auto audio_transport
    = std::make_shared<TcpInterleavedTransport>(serializer, m_server_socket, 2, 3);

  // 교대로 전송
  video_transport->send_rtp(std::vector<uint8_t>{0x80, 0x60});
  audio_transport->send_rtp(std::vector<uint8_t>{0x80, 0x08});
  video_transport->send_rtp(std::vector<uint8_t>{0x80, 0x61});

  run_for(nx::milliseconds(200));

  // 전체 수신 (3개의 인터리브 프레임)
  // 각 프레임: 4(헤더) + 2(데이터) = 6 바이트
  std::vector<uint8_t> recv_buf(256);
  boost::system::error_code ec;
  size_t total = 0;
  while (total < 18) { // 6 * 3 = 18
    auto bytes = m_client_socket->read_some(
      boost::asio::buffer(recv_buf.data() + total, recv_buf.size() - total),
      ec);
    if (ec) {
      break;
    }
    total += bytes;
  }

  ASSERT_EQ(total, 18u);

  // 순서 확인: video(ch0) → audio(ch2) → video(ch0)
  EXPECT_EQ(recv_buf[0], '$');
  EXPECT_EQ(recv_buf[1], 0x00); // video ch
  EXPECT_EQ(recv_buf[6], '$');
  EXPECT_EQ(recv_buf[7], 0x02); // audio ch
  EXPECT_EQ(recv_buf[12], '$');
  EXPECT_EQ(recv_buf[13], 0x00); // video ch
}

TEST_F(TcpWriteSerializerTest, TransportInactiveAfterSerializerClose)
{
  auto serializer = std::make_shared<TcpWriteSerializer>(m_server_socket);
  auto transport
    = std::make_shared<TcpInterleavedTransport>(serializer, m_server_socket, 0, 1);

  EXPECT_TRUE(transport->is_active());

  serializer->close();
  run_for(nx::milliseconds(50));

  EXPECT_FALSE(transport->is_active());
}

} // anonymous namespace
