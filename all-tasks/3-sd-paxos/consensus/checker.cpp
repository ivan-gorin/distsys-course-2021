#include <consensus/checker.hpp>

#include <consensus/value.hpp>

#include <set>

namespace consensus {

//////////////////////////////////////////////////////////////////////

class SafetyChecker {
 public:
  explicit SafetyChecker(const whirl::semantics::History& history) {
    Load(history);
  }

  bool Safe() const {
    return Agreement() && Validity();
  }

  bool Agreement() const {
    return outputs_.size() == 1;
  }

  bool Validity() const {
    if (outputs_.empty()) {
      return true;
    }
    const Value output = *outputs_.begin();
    return inputs_.count(output) > 0;
  }

 private:
  void Load(const whirl::semantics::History& history) {
    for (const auto& call : history) {
      auto [input] = call.arguments.As<Value>();
      inputs_.insert(input);

      if (call.IsCompleted()) {
        auto output = call.result->As<Value>();
        outputs_.insert(output);
      }
    }
  }

 private:
  std::set<Value> inputs_;
  std::set<Value> outputs_;
};

//////////////////////////////////////////////////////////////////////

bool IsSafe(const whirl::semantics::History& history) {
  SafetyChecker checker(history);
  return checker.Safe();
}

}   // namespace consensus
