#include <runtime/mt/runtime.hpp>

#include <await/fibers/static/services.hpp>

namespace runtime::mt {

Runtime::Runtime(size_t threads)
    : scheduler_(threads, "scheduler"),
      nursery_(await::fibers::GlobalManager(), &scheduler_) {
}

void Runtime::Join() {
  nursery_.Join();
  scheduler_.Join();
}

}  // namespace runtime::mt
