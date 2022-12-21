#include <runtime/matrix/timers.hpp>

using await::futures::Future;

namespace runtime::matrix {

Future<void> TimerService::AfterJiffies(await::time::Jiffies delay) {
  return AfterImpl(std::chrono::milliseconds(delay));
}

Future<void> TimerService::AfterImpl(Duration delay) {
  auto deadline = clock_.ToDeadLine(delay);

  auto [f, p] = await::futures::MakeContract<void>();
  timers_.push({deadline, std::move(p)});
  return std::move(f);
}

std::vector<TimerService::TimerPromise> TimerService::GrabReadyTimers() {
  auto now = clock_.Now();

  std::vector<TimerPromise> promises;

  while (!timers_.empty()) {
    auto& next = timers_.top();
    if (next.deadline > now) {
      break;
    }
    promises.push_back(std::move(next.promise));
    timers_.pop();
  }

  return promises;
}

size_t TimerService::Poll() {
  auto promises = GrabReadyTimers();

  for (auto&& p : promises) {
    std::move(p).Set();
  }
  return promises.size();
}

}  // namespace runtime::matrix
