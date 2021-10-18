#pragma once

#include <whirl/node/time/jiffies.hpp>
#include <whirl/node/runtime/shortcuts.hpp>

namespace paxos {

struct Backoff {
  using Jiffies = whirl::Jiffies;

 public:
  struct Params {
    Jiffies init;
    Jiffies max;
    int factor;
  };

 public:
  explicit Backoff(Params params) : params_(params), next_(params.init) {
  }

  // Returns backoff delay
  Jiffies operator()() {
    auto curr = next_;
    next_ = ComputeNext(curr);
    return curr;
  }

  Jiffies Next() {
    return std::exchange(next_, ComputeNext(next_));
  }

 private:
  Jiffies ComputeNext(Jiffies curr) {
    size_t random_factor = (double)1000 / whirl::node::rt::RandomNumber(1000);
    return std::min(params_.max, curr * params_.factor * random_factor);
  }

 private:
  const Params params_;
  Jiffies next_;
};

}  // namespace paxos
