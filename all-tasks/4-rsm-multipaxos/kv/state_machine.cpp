#include <kv/state_machine.hpp>

#include <kv/operations.hpp>
#include <kv/store.hpp>

#include <muesli/serialize.hpp>

#include <cereal/types/string.hpp>
#include <cereal/types/utility.hpp>
#include <cereal/types/vector.hpp>

#include <wheels/support/panic.hpp>

namespace kv {

class StateMachine : public rsm::IStateMachine {
 public:
  // IStateMachine

  muesli::Bytes Apply(const rsm::Command& cmd) override {
    if (cmd.type == "Set") {
      return Apply<Set>(cmd);
    } else if (cmd.type == "Get") {
      return Apply<Get>(cmd);
    } else if (cmd.type == "Cas") {
      return Apply<Cas>(cmd);
    }
    WHEELS_PANIC("Unknown command type: " << cmd.type);
  }

  void Reset() override {
    store_.Clear();
  }

  muesli::Bytes MakeSnapshot() override {
    return muesli::Serialize(store_.MakeSnapshot());
  }

  void InstallSnapshot(const muesli::Bytes& snapshot) override {
    auto entries = muesli::Deserialize<Store::EntriesList>(snapshot);
    store_.Install(entries);
  }

 private:
  Set::Response ApplyImpl(Set::Request set) {
    store_.Set(set.key, set.value);
    return {};
  }

  Get::Response ApplyImpl(Get::Request get) {
    return {store_.Get(get.key)};
  }

  Cas::Response ApplyImpl(Cas::Request cas) {
    return {store_.Cas(cas.key, cas.expected_value, cas.target_value)};
  }

  template <typename Op>
  muesli::Bytes Apply(const rsm::Command& cmd) {
    auto request = muesli::Deserialize<typename Op::Request>(cmd.request);
    auto response = ApplyImpl(request);
    return muesli::Serialize(response);
  }

 private:
  Store store_;
};

rsm::IStateMachinePtr MakeStateMachine() {
  return std::make_shared<StateMachine>();
}

}  // namespace kv
