#pragma once

#include <rpc/time_units.hpp>

#include <algorithm>
#include <chrono>

namespace rpc {

// No jitter =(
// https://aws.amazon.com/blogs/architecture/exponential-backoff-and-jitter/

struct Backoff {
  using Millis = std::chrono::milliseconds;

 public:
  struct Params {
    Millis init;
    Millis max;
    int factor;
  };

 public:
  explicit Backoff(Params params) : params_(params), next_(params.init) {
  }

  // Returns backoff delay
  Millis operator()() {
    auto curr = next_;
    next_ = ComputeNext(curr);
    return curr;
  }

 private:
  Millis ComputeNext(Millis curr) {
    return std::min(params_.max, curr * params_.factor);
  }

 private:
  const Params params_;
  Millis next_;
};

}  // namespace rpc
