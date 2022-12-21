#pragma once

#include <rsm/replica/paxos/proposal.hpp>
#include <rsm/replica/paxos/proto.hpp>
#include <rsm/replica/store/log.hpp>

#include <commute/rpc/service_base.hpp>

#include <timber/logger.hpp>
#include <whirl/node/store/struct.hpp>
#include <await/fibers/sync/mutex.hpp>

namespace paxos {

// Acceptor role / RPC service

class Acceptor : public commute::rpc::ServiceBase<Acceptor> {
 public:
  explicit Acceptor(rsm::Log& log, await::fibers::Mutex& log_mutex);

 protected:
  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_HANDLER(Prepare);
    COMMUTE_RPC_REGISTER_HANDLER(Accept);
  }

  // Phase 1 (Prepare / Promise)

  void Prepare(const proto::Prepare::Request& request,
               proto::Prepare::Response* response);

  // Phase 2 (Accept / Accepted)

  void Accept(const proto::Accept::Request& request,
              proto::Accept::Response* response);

 private:
  timber::Logger logger_;
  rsm::Log log_;
  await::fibers::Mutex mutex_;
  await::fibers::Mutex& log_mutex_;

  void UpdatePrepare(ProposalNumber new_number, size_t log_index);
  void UpdateAccept(Proposal new_proposal, size_t log_index);
};

}  // namespace paxos
