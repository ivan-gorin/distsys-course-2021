#pragma once

#include <await/executors/executor.hpp>
#include <await/time/timer_service.hpp>

#include <timber/backend.hpp>

namespace rpc {

struct IRuntime {
  virtual ~IRuntime() = default;

  // Execution
  virtual await::executors::IExecutor* Executor() = 0;

  // Timers
  virtual await::time::ITimerService* Timers() = 0;

  // Logging
  virtual timber::ILogBackend* Log() = 0;
};

}  // namespace rpc
