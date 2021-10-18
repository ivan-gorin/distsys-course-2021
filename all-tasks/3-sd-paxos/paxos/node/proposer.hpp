#pragma once

#include <paxos/node/proposal.hpp>
#include <paxos/node/proto.hpp>
#include <paxos/node/backoff.hpp>

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

  Value Propose(Value input);

 private:
  timber::Logger logger_;
  twist::stdlike::atomic<uint64_t> num_{0};

  Value Phase1(Value input);
  std::optional<Value> Phase2(Value input, ProposalNumber n);
};

}  // namespace paxos
