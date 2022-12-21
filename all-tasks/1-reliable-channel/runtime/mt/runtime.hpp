#pragma once

#include <runtime/mt/log.hpp>

#include <rpc/runtime.hpp>

#include <await/executors/static_thread_pool.hpp>
#include <await/fibers/sync/nursery.hpp>
#include <await/time/time_keeper.hpp>

#include <timber/logger.hpp>

namespace runtime::mt {

// Multi-threaded runtime

class Runtime : public rpc::IRuntime {
 public:
  explicit Runtime(size_t threads);

  // Spawn new fiber
  template <typename F>
  void Spawn(F&& routine) {
    nursery_.Spawn(std::forward<F>(routine));
  }

  void Join();

  // IRuntime

  await::executors::IExecutor* Executor() override {
    return &scheduler_;
  }

  await::time::ITimerService* Timers() override {
    return &timers_;
  };

  timber::ILogBackend* Log() override {
    return &log_;
  }

 private:
  await::executors::StaticThreadPool scheduler_;
  await::fibers::Nursery nursery_;
  await::time::TimeKeeper timers_;
  LogBackend log_;
};

}  // namespace runtime::mt
