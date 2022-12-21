#include <runtime/matrix/matrix.hpp>

#include <timber/log.hpp>

namespace runtime::matrix {

bool Matrix::KeepRunning() const {
  return (tasks_.TaskCount() > 0) || timers_.HasTimers();
}

void Matrix::RunLoop() {
  LOG_INFO("Simulation '{}' started", name_);

  while (KeepRunning()) {
    LOG_INFO("Drain task queue");
    size_t task_count = tasks_.Drain();
    LOG_INFO("Tasks completed: {}", task_count);

    if (timers_.HasTimers()) {
      auto next_deadline = timers_.NextDeadLine();

      LOG_INFO("Fast-forward time to {}", next_deadline);
      clock_.FastForwardTo(next_deadline);

      LOG_INFO("Poll ready timers");
      size_t timer_count = timers_.Poll();
      LOG_INFO("Timers completed: {}", timer_count);
    }
    timers_.Poll();
  }

  LOG_INFO("Simulation completed");
}

}  // namespace runtime::matrix
