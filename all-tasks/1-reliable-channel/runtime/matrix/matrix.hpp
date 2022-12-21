#pragma once

#include <runtime/matrix/clock.hpp>
#include <runtime/matrix/timers.hpp>
#include <runtime/matrix/log.hpp>

#include <await/executors/executor.hpp>
#include <await/executors/manual.hpp>

#include <await/fibers/core/api.hpp>
#include <await/time/timer_service.hpp>

#include <timber/backend.hpp>
#include <timber/logger.hpp>

#include <rpc/runtime.hpp>

#include <cassert>
#include <chrono>
#include <queue>
#include <vector>

namespace runtime::matrix {

// Single-threaded deterministic simulation

class Matrix : public rpc::IRuntime {
 public:
  explicit Matrix(std::string name)
      : name_(std::move(name)),
        timers_(clock_),
        log_(clock_),
        logger_("Runtime", &log_) {
  }

  // Spawn initial fiber and run simulation
  template <typename F>
  Clock::TimePoint Run(F&& init) {
    await::fibers::Start(Executor(), std::forward<F>(init));
    RunLoop();
    return clock_.Now();
  }

  Clock::TimePoint Now() const {
    return clock_.Now();
  }

  // IRuntime

  await::executors::IExecutor* Executor() override {
    return &tasks_;
  }

  await::time::ITimerService* Timers() override {
    return &timers_;
  }

  timber::ILogBackend* Log() override {
    return &log_;
  }

 private:
  void RunLoop();
  bool KeepRunning() const;

 private:
  const std::string name_;
  Clock clock_;
  await::executors::ManualExecutor tasks_;
  TimerService timers_;
  LogBackend log_;
  timber::Logger logger_;
};

}  // namespace runtime::matrix
