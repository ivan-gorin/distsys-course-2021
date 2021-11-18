#include <rsm/replica/paxos/acceptor.hpp>
#include <whirl/node/runtime/shortcuts.hpp>

#include <timber/log.hpp>

using namespace whirl;

namespace paxos {

Acceptor::Acceptor(rsm::Log& log, await::fibers::Mutex& log_mutex)
    : logger_("Paxos.Acceptor", node::rt::LoggerBackend()),
      log_(log),
      log_mutex_(log_mutex) {
}

void Acceptor::Prepare(const proto::Prepare::Request& request,
                       proto::Prepare::Response* response) {
  std::lock_guard<await::fibers::Mutex> lock(log_mutex_);
  auto log_value = log_.Read(request.log_index);
  if (!log_value.has_value()) {
    UpdatePrepare(request.n, request.log_index);
  } else if (log_value->prepare < request.n) {
    UpdatePrepare(request.n, request.log_index);
    if (log_value->proposal.has_value()) {
      response->vote = log_value->proposal.value();
    }
  } else {
    response->ack = false;
    response->advice = log_value->prepare;
  }
}

void Acceptor::Accept(const proto::Accept::Request& request,
                      proto::Accept::Response* response) {
  std::lock_guard<await::fibers::Mutex> lock(log_mutex_);
  auto log_value = log_.Read(request.log_index);
  if (!log_value.has_value() || log_value->prepare <= request.proposal.n) {
    UpdateAccept(request.proposal, request.log_index);
    response->ack = true;
  } else {
    response->ack = false;
    response->advice = log_value->prepare;
  }
}

void Acceptor::UpdatePrepare(ProposalNumber new_number, size_t log_index) {
  rsm::LogEntry new_entry;
  if (!log_.IsEmpty(log_index)) {
    new_entry = log_.Read(log_index).value();
  }
  new_entry.prepare = new_number;
  log_.Update(log_index, new_entry);
}

void Acceptor::UpdateAccept(Proposal new_proposal, size_t log_index) {
  rsm::LogEntry new_entry;
  if (!log_.IsEmpty(log_index)) {
    new_entry = log_.Read(log_index).value();
  }
  new_entry.proposal = new_proposal;
  new_entry.prepare = new_proposal.n;
  log_.Update(log_index, new_entry);
}

}  // namespace paxos
