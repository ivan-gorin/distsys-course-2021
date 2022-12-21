#include <runtime/mt/log.hpp>

#include <await/fibers/core/api.hpp>

#include <timber/level/ostream.hpp>

#include <fmt/core.h>

#include <thread>

namespace runtime::mt {

static std::string ThisFiberName() {
  if (await::fibers::AmIFiber()) {
    return fmt::format("Fiber-{}", await::fibers::self::GetId());
  } else {
    return "-";
  }
}

void LogBackend::Log(timber::Event event) {
  std::lock_guard guard(log_mutex_);

  std::cout << event.level << "\t" << event.component << "\t"
            << std::this_thread::get_id() << '\t' << ThisFiberName() << '\t'
            << event.message << std::endl;
}

}  // namespace runtime::mt
