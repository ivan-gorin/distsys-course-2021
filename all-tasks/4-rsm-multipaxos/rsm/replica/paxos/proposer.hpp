#pragma once

#include <rsm/replica/paxos/proposal.hpp>
#include <rsm/replica/paxos/proposal.hpp>
#include <rsm/replica/paxos/proto.hpp>
#include <rsm/replica/paxos/backoff.hpp>

#include <commute/rpc/service_base.hpp>
#include <commute/rpc/call.hpp>

#include <commute/rpc/client.hpp>

#include <await/futures/combine/quorum.hpp>

#include <whirl/node/cluster/peer.hpp>
#include <await/time/timer_service.hpp>

#include <timber/logger.hpp>

namespace paxos {

// Proposer role / RPC service

class Proposer : public commute::rpc::ServiceBase<Proposer>,
                 public whirl::node::cluster::Peer {
 public:
  Proposer();

 protected:
  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_METHOD(Propose);
  }

 private:
  timber::Logger logger_;

  Value Propose(Value input, size_t log_index);
  twist::stdlike::atomic<uint64_t> num_{0};
  Value Phase1(Value input, size_t log_index);
  std::optional<Value> Phase2(Value input, ProposalNumber n, size_t log_index);
};

}  // namespace paxos
