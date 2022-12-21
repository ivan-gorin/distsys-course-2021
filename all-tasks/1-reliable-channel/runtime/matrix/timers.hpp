#pragma once

#include <runtime/matrix/clock.hpp>

#include <await/time/timer_service.hpp>
#include <await/futures/core/future.hpp>

#include <queue>
#include <vector>

namespace runtime::matrix {

class TimerService : public await::time::ITimerService {
  using TimePoint = Clock::TimePoint;
  using Duration = Clock::Duration;

  using TimerPromise = await::futures::Promise<void>;

  struct Timer {
    TimePoint deadline;
    mutable TimerPromise promise;

    bool operator<(const Timer& rhs) const {
      return deadline > rhs.deadline;
    }
  };

 public:
  explicit TimerService(Clock& clock) : clock_(clock) {
  }

  bool HasTimers() const {
    return !timers_.empty();
  }

  // Precondition: HasTimers() == true
  TimePoint NextDeadLine() const {
    return timers_.top().deadline;
  }

  // Returns number of completed timers
  size_t Poll();

  // ITimerService

  await::futures::Future<void> AfterJiffies(
      await::time::Jiffies delay) override;

 private:
  await::futures::Future<void> AfterImpl(Duration delay);
  std::vector<TimerPromise> GrabReadyTimers();

 private:
  Clock& clock_;
  std::priority_queue<Timer> timers_;
};

}  // namespace runtime::matrix
