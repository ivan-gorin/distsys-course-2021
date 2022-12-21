#pragma once

#include <await/time/jiffies.hpp>

#include <chrono>

namespace await::time {

// await::time::Jiffies = std::chrono::milliseconds

template <>
inline Jiffies ToJiffies(std::chrono::milliseconds dur) {
  return dur.count();
}

template <>
inline Jiffies ToJiffies(std::chrono::seconds dur) {
  return std::chrono::milliseconds(dur).count();
}

}  // namespace await::time
