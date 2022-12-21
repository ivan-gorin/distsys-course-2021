#pragma once

#include <rsm/replica/replica.hpp>

#include <commute/rpc/service_base.hpp>

#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/future.hpp>

namespace rsm {

class ReplicaService : public commute::rpc::ServiceBase<ReplicaService> {
 public:
  explicit ReplicaService(IReplicaPtr replica) : replica_(std::move(replica)) {
  }

  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_METHOD(Execute);
    COMMUTE_RPC_REGISTER_METHOD(Heartbeat);
  }

 protected:
  Response Execute(Command command) {
    return await::fibers::Await(replica_->Execute(std::move(command)))
        .ValueOrThrow();
  }

  void Heartbeat(std::string hostname) {
    replica_->UpdateLeader(hostname);
  }

 private:
  IReplicaPtr replica_;
};

}  // namespace rsm
