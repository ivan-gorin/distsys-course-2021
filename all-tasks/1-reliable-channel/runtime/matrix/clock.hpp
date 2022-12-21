#pragma once

#include <cassert>
#include <cstdlib>
#include <chrono>

namespace runtime::matrix {

// Virtual time

class Clock {
 public:
  using TimePoint = uint64_t;
  using Duration = std::chrono::milliseconds;

 public:
  Clock() = default;

  TimePoint Now() const {
    return now_;
  }

  void FastForwardTo(TimePoint future) {
    assert(future >= now_);
    now_ = future;
  }

  TimePoint ToDeadLine(Duration delay) const {
    return now_ + delay.count();
  }

 private:
  TimePoint now_{0};
};

}  // namespace runtime::matrix
