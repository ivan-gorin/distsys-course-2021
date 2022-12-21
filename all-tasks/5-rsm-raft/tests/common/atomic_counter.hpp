#pragma once

#include <kv/client.hpp>

namespace tests {

class AtomicCounter {
  using CounterType = uint64_t;

 public:
  AtomicCounter(kv::Client& client, const std::string& name)
      : client_(client), key_(std::move(name)) {
  }

  size_t FetchAdd(CounterType d) {
    while (true) {
      size_t current = Get();
      if (Cas(current, current + d) == current) {
        return current;
      }
    }
  }

 private:
  CounterType Cas(CounterType expected, CounterType target) {
    kv::Value prev_value =
        client_.Cas(key_, ToValue(expected), ToValue(target));
    return FromValue(prev_value);
  }

  CounterType Get() {
    return FromValue(client_.Get(key_));
  }

  static CounterType FromValue(kv::Value value) {
    if (value.empty()) {
      return 0;
    }
    return std::stoi(value);
  }

  static kv::Value ToValue(CounterType value) {
    if (value == 0) {
      return "";
    }
    return std::to_string(value);
  }

 private:
  kv::Client& client_;
  std::string key_;
};

}  // namespace tests