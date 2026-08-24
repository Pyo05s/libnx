// 파일: asio_type.h
// 생성일: 2026-06-17
// 설명: 자주 사용하는 Boost.Asio 관련 타입 별칭 정의

#pragma once

#include <boost/asio.hpp>
#include <boost/asio/experimental/channel.hpp>
#include <boost/asio/experimental/concurrent_channel.hpp>

#include <boost/system/error_code.hpp>

using AsioContext = boost::asio::io_context;
using AsioSteadyTimer = boost::asio::steady_timer;
using AsioStrand = boost::asio::strand<boost::asio::any_io_executor>;
using AsioWorkGuard
  = boost::asio::executor_work_guard<boost::asio::io_context::executor_type>;

template <typename ExecutorOrSignature, typename... Signatures>
using AsioChannel
  = boost::asio::experimental::channel<ExecutorOrSignature, Signatures...>;

template <typename ExecutorOrSignature, typename... Signatures>
using AsioConcurrentChannel
  = boost::asio::experimental::concurrent_channel<ExecutorOrSignature, Signatures...>;