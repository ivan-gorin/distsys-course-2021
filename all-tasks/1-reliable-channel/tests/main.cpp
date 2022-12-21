#include "fair_loss.hpp"
#include "assert.hpp"

#include <rpc/backoff.hpp>
#include <rpc/reliable.hpp>
#include <rpc/errors.hpp>

#include <runtime/matrix/matrix.hpp>
#include <runtime/mt/runtime.hpp>

// Concurrency
#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/future.hpp>
#include <await/fibers/sync/nursery.hpp>
#include <await/fibers/sync/spawn.hpp>
#include <await/futures/combine/first_of.hpp>
#include <await/context/stop_token.hpp>

// Logging
#include <timber/log.hpp>

#include <wheels/support/result.hpp>
#include <wheels/support/stop_watch.hpp>

#include <iostream>
#include <chrono>

using await::fibers::Await;
using await::futures::Future;
using wheels::Result;

using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

ITestServicePtr MakeEchoService() {
  auto echo = std::make_shared<TestService>("EchoService");

  echo->Add("Echo", [](std::string request) {
    return request;
  });

  return echo;
}

//////////////////////////////////////////////////////////////////////

// Deterministic tests

//////////////////////////////////////////////////////////////////////

void MatrixTest1() {
  runtime::matrix::Matrix matrix{"Test-1"};

  matrix.Run([&]() {
    // Test routine

    timber::Logger logger_("Test", matrix.Log());

    auto echo = MakeEchoService();

    auto fair_loss = MakeFairLossChannel(echo, /*fails=*/3, &matrix);

    auto reliable = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{100ms, 1s, 2}, &matrix);

    // Non-blocking
    auto future =
        reliable->Call("Echo", "request", {await::context::NeverStop()});

    // No blocking calls in caller
    TEST_ASSERT(matrix.Now() == 0);

    auto result = Await(std::move(future));

    TEST_ASSERT(result.IsOk());
    TEST_ASSERT(*result == "request");

    // 0 + 100 + 200 + 400
    TEST_ASSERT(matrix.Now() == 700);
  });

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MatrixTest2() {
  runtime::matrix::Matrix matrix{"Test-2"};

  auto end_time = matrix.Run([&]() {
    timber::Logger logger_("Test", matrix.Log());

    auto echo = MakeEchoService();

    auto fair_loss_1 = MakeFairLossChannel(echo, /*fails=*/2, &matrix);
    auto fair_loss_2 = MakeFairLossChannel(echo, /*fails=*/123456789, &matrix);

    auto reliable_1 = MakeReliableChannel(
        fair_loss_1, rpc::Backoff::Params{100ms, 1s, 2}, &matrix);

    auto reliable_2 = MakeReliableChannel(
        fair_loss_2, rpc::Backoff::Params{100ms, 1s, 3}, &matrix);

    await::context::StopScope stop_scope;

    auto f_1 = reliable_1->Call("Echo", "test", {stop_scope.GetToken()});
    auto f_2 = reliable_2->Call("Echo", "test", {stop_scope.GetToken()});

    auto first = await::futures::FirstOf(std::move(f_1), std::move(f_2));

    TEST_ASSERT(matrix.Now() == 0);

    auto result = Await(std::move(first));

    // 0 + 100 + 200
    TEST_ASSERT(matrix.Now() == 300);
    TEST_ASSERT(result.ValueOrThrow() == "test");
  });

  TEST_ASSERT(end_time == 400);

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MatrixTest3() {
  runtime::matrix::Matrix matrix{"Test-3"};

  matrix.Run([&]() {
    timber::Logger logger_("Test", matrix.Log());

    auto echo = MakeEchoService();

    // IILE
    auto future = [&]() {
      auto reliable = MakeReliableChannel(
          MakeFairLossChannel(echo, /*fails=*/13, &matrix),  //
          rpc::Backoff::Params{100ms, 100ms, 1}, &matrix);

      return reliable->Call("Echo", "Some long message with some words in it",
                            {await::context::NeverStop()});
    }();

    auto result = Await(std::move(future));

    TEST_ASSERT(result.IsOk());
    TEST_ASSERT(*result == "Some long message with some words in it");

    // 13 * 100
    TEST_ASSERT(matrix.Now() == 1300);
  });

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MatrixTest4() {
  runtime::matrix::Matrix matrix{"Test-4"};

  matrix.Run([&]() {
    timber::Logger logger_("Test", matrix.Log());

    auto echo = MakeEchoService();

    auto fair_loss = MakeFairLossChannel(echo, /*fails=*/6, &matrix);

    auto reliable_1 = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{40ms, 1s, 2}, &matrix);

    auto reliable_2 = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{30ms, 1s, 2}, &matrix);

    await::context::StopScope stop_scope;

    auto f1 = reliable_1->Call("Echo", "", {stop_scope.GetToken()});
    auto f2 = reliable_2->Call("Echo", "", {stop_scope.GetToken()});

    TEST_ASSERT(matrix.Now() == 0);

    auto first = await::futures::FirstOf(std::move(f1), std::move(f2));

    Await(std::move(first)).ExpectOk();

    // Call 1: 0, 30, 90, 210
    // Call 2: 0, 40, 120
    TEST_ASSERT(matrix.Now() == 210);
  });

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MatrixTest5() {
  runtime::matrix::Matrix matrix{"Test-5"};

  matrix.Run([&]() {
    timber::Logger logger_("Test", matrix.Log());

    auto echo = MakeEchoService();

    auto fair_loss = MakeFairLossChannel(echo, /*fails=*/2, &matrix);

    auto reliable = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{200ms, 1s, 2}, &matrix);

    auto stop_token = await::context::NeverStop();

    // First
    auto rsp1 = Await(reliable->Call("Echo", "first", {stop_token})).ValueOrThrow();

    TEST_ASSERT(rsp1 == "first");
    // 200 + 400
    TEST_ASSERT(matrix.Now() == 600);

    // Second
    auto rsp2 = Await(reliable->Call("Echo", "second", {stop_token})).ValueOrThrow();

    TEST_ASSERT(rsp2 == "second");
    // 200 + 400
    TEST_ASSERT(matrix.Now() == 1200);
  });

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

ITestServicePtr MakePingService() {
  auto service = std::make_shared<TestService>("PingService");
  service->Add("Ping", [](std::string /*request*/) {
    return "pong";
  });
  return service;
}

//////////////////////////////////////////////////////////////////////

// Multi-threaded non-deterministic tests

//////////////////////////////////////////////////////////////////////

void MTTest1() {
  runtime::mt::Runtime mt(/*threads=*/4);

  mt.Spawn([&]() {
    timber::Logger logger_("Test", mt.Log());

    auto ping = MakePingService();

    auto fair_loss = MakeFairLossChannel(ping, /*fails=*/10, &mt);

    auto reliable = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{100ms, 500ms, 2}, &mt);

    auto future =
        reliable->Call("Ping", "request", {await::context::NeverStop()});

    wheels::StopWatch stop_watch;

    auto result = await::fibers::Await(std::move(future));

    TEST_ASSERT(result.ValueOrThrow() == "pong");
    TEST_ASSERT(stop_watch.Elapsed() >= 3s);
  });

  mt.Join();

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MTTest2() {
  runtime::mt::Runtime mt(/*threads=*/4);

  mt.Spawn([&]() {
    timber::Logger logger_("Test", mt.Log());

    auto ping = MakePingService();

    auto fair_loss = MakeFairLossChannel(
        ping,
        /*fails=*/std::numeric_limits<size_t>::max(),  // Failing infinitely
        &mt);

    auto reliable = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{100ms, 500ms, 2}, &mt);

    // IILE
    auto future = [&] {
      await::context::StopScope stop_scope;

      auto future = reliable->Call("Ping", "request", {stop_scope.GetToken()});

      Await(mt.Timers()->After(3s)).ExpectOk();

      return future;
    }();

    auto result = Await(std::move(future));

    TEST_ASSERT(result.HasError());
    TEST_ASSERT(result.GetErrorCode() == rpc::Cancelled());
  });

  mt.Join();

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

void MTTest3() {
  runtime::mt::Runtime mt(/*threads=*/4);

  mt.Spawn([&]() {
    timber::Logger logger_("Test", mt.Log());

    auto ping = MakePingService();

    auto fair_loss = MakeFairLossChannel(
        ping, /*fails=*/5, &mt);

    auto reliable = MakeReliableChannel(
        fair_loss, rpc::Backoff::Params{10ms, 30ms, 2}, &mt);

    {
      await::fibers::Nursery nursery;

      for (size_t i = 0; i < 123; ++i) {
        nursery.Spawn([&, reliable]() {
          auto f = reliable->Call("Ping", "request", {nursery.GetToken()});
          auto r = Await(std::move(f));
          TEST_ASSERT(r.ValueOrThrow() == "pong");
        });
      }
    }

  });

  mt.Join();

  std::cout << std::endl;
}

//////////////////////////////////////////////////////////////////////

int main() {
  // Deterministic tests

  MatrixTest1();
  MatrixTest2();
  MatrixTest3();
  MatrixTest4();
  MatrixTest5();

  // Multi-threaded tests

  MTTest1();
  MTTest2();
  MTTest3();

  return 0;
}
