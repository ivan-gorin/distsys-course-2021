#pragma once

#include <rsm/replica/state_machine.hpp>

namespace kv {

rsm::IStateMachinePtr MakeStateMachine();

}  // namespace kv
