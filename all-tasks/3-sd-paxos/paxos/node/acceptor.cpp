#include <paxos/node/acceptor.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

#include <timber/log.hpp>

using namespace whirl;

namespace paxos {

Acceptor::Acceptor()
    : logger_("Paxos.Acceptor", node::rt::LoggerBackend()),
      struct_store_(node::rt::Database(), "prepare") {
}

void Acceptor::Prepare(const proto::Prepare::Request& request,
                       proto::Prepare::Response* response) {
  std::lock_guard<await::fibers::Mutex> lock(mutex_);
  std::optional<ProposalNumber> local_prepare =
      struct_store_.TryLoad<ProposalNumber>("prepare");
  if (!local_prepare.has_value() || local_prepare.value() < request.n) {
    UpdatePrepare(request.n);
    response->ack = true;
    std::optional<Proposal> local_proposal =
        struct_store_.TryLoad<Proposal>("proposal");
    if (local_proposal.has_value()) {
      response->vote = local_proposal;
    }
  } else {
    response->ack = false;
    response->advice = local_prepare.value();
  }
}

void Acceptor::Accept(const proto::Accept::Request& request,
                      proto::Accept::Response* response) {
  std::lock_guard<await::fibers::Mutex> lock(mutex_);
  std::optional<ProposalNumber> local_prepare =
      struct_store_.TryLoad<ProposalNumber>("prepare");
  if (!local_prepare.has_value() ||
      local_prepare.value() <= request.proposal.n) {
    UpdatePrepare(request.proposal.n);
    UpdateAccept(request.proposal);
    response->ack = true;
  } else {
    response->ack = false;
    response->advice = local_prepare.value();
  }
}

void Acceptor::UpdatePrepare(ProposalNumber new_number) {
  struct_store_.Store("prepare", new_number);
}

void Acceptor::UpdateAccept(Proposal new_proposal) {
  struct_store_.Store("proposal", new_proposal);
}

}  // namespace paxos
