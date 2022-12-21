#pragma once

#include <rsm/client/command.hpp>
#include <rsm/replica/proto/response.hpp>

#include <await/futures/core/future.hpp>

#include <memory>

namespace rsm {

//////////////////////////////////////////////////////////////////////

struct IReplica {
  virtual ~IReplica() = default;

  virtual await::futures::Future<proto::Response> Execute(Command command) = 0;
};

using IReplicaPtr = std::shared_ptr<IReplica>;

}  // namespace rsm
