#include <runtime/matrix/log.hpp>

#include <await/fibers/core/api.hpp>

#include <timber/level/format.hpp>

#include <fmt/core.h>

namespace runtime::matrix {

static std::string ThisFiberName() {
  if (await::fibers::AmIFiber()) {
    return fmt::format("Fiber-{}", await::fibers::self::GetId());
  } else {
    return "-";
  }
}

void LogBackend::Log(timber::Event event) {
  fmt::print("[T {:<4}] -- {:<5} -- {:<10} -- {:<7} -- {}\n", clock_.Now(),
             event.level, event.component, ThisFiberName(), event.message);
}

}  // namespace runtime::matrix
