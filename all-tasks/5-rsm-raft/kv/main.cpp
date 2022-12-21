#include <kv/main.hpp>
#include <kv/state_machine.hpp>

#include <rsm/replica/main.hpp>

namespace kv {

void ReplicaMain() {
  rsm::ReplicaMain(MakeStateMachine());
}

}  // namespace kv
