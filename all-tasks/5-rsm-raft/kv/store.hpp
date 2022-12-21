#pragma once

#include <kv/types.hpp>

#include <map>
#include <vector>

namespace kv {

// In-memory KV store

class Store {
 public:
  using EntriesList = std::vector<std::pair<Key, Value>>;

 public:
  explicit Store(Value default_value = "")
      : default_value_(std::move(default_value)) {
  }

  void Set(const Key& key, const Value& value) {
    entries_.insert_or_assign(key, value);
  }

  Value Get(const Key& key) const {
    if (auto it = entries_.find(key); it != entries_.end()) {
      return it->second;
    } else {
      return default_value_;
    }
  }

  // Compare-and-set

  Value Cas(const Key& key, const Value& expected_value,
            const Value& target_value) {
    auto old_value = Get(key);
    if (old_value == expected_value) {
      Set(key, target_value);
    }
    return old_value;
  }

  void Clear() {
    entries_.clear();
  }

  EntriesList MakeSnapshot() {
    EntriesList entries;
    for (const auto& [k, v] : entries_) {
      entries.emplace_back(k, v);
    }
    return entries;
  }

  void Install(const EntriesList& entries) {
    Clear();
    for (const auto& [k, v] : entries) {
      entries_.emplace(k, v);
    }
  }

  auto begin() const {
    return entries_.begin();
  }

  auto end() const {
    return entries_.end();
  }

 private:
  const Value default_value_;
  std::map<Key, Value> entries_;
};

}  // namespace kv