#pragma once

#include <rsm/client/command.hpp>
#include <rsm/replica/response.hpp>

#include <await/futures/core/future.hpp>

#include <memory>

namespace rsm {

//////////////////////////////////////////////////////////////////////

struct IReplica {
  virtual ~IReplica() = default;

  virtual await::futures::Future<Response> Execute(Command command) = 0;
  virtual void UpdateLeader(std::string& new_leader) = 0;
};

using IReplicaPtr = std::shared_ptr<IReplica>;

}  // namespace rsm
