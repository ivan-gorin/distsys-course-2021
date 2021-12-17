#pragma once

#include <rsm/client/command.hpp>

#include <muesli/bytes.hpp>

#include <string>
#include <memory>

namespace rsm {

// NOT thread-safe!

struct IStateMachine {
  virtual ~IStateMachine() = default;

  // Move state machine to initial state
  virtual void Reset() = 0;

  // Applies command
  // Returns serialized operation response
  virtual muesli::Bytes Apply(Command command) = 0;

  // Snapshots

  virtual muesli::Bytes MakeSnapshot() = 0;
  virtual void InstallSnapshot(muesli::Bytes snapshot) = 0;
};

using IStateMachinePtr = std::shared_ptr<IStateMachine>;

}  // namespace rsm
