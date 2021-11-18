#include <rsm/replica/paxos/proposer.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

#include <timber/log.hpp>

using namespace whirl;
using await::fibers::Await;
using await::futures::Future;

namespace paxos {

Proposer::Proposer()
    : Peer(node::rt::Config()),
      logger_("Paxos.Proposer", node::rt::LoggerBackend()) {
}

Value Proposer::Propose(Value input, size_t log_index) {
  return Phase1(input, log_index);
}

Value Proposer::Phase1(Value input, size_t log_index) {
  paxos::Backoff::Params params{1, 10, 2};
  paxos::Backoff backoff(params);
  while (true) {
    // ProposalNumber = {num_, node.id}
    ProposalNumber n{num_.fetch_add(1),
                     (uint64_t)node::rt::Config()->GetInt64("node.id")};
    // Request = {ProposalNumber, log_index}
    proto::Prepare::Request request{n, log_index};
    std::vector<Future<proto::Prepare::Response>> prepares;

    // Call Prepare on all Acceptors
    for (const auto& peer : ListPeers().WithMe()) {
      prepares.push_back(commute::rpc::Call("Acceptor.Prepare")
                             .Args(request)
                             .Via(Channel(peer))
                             .Start()
                             .As<proto::Prepare::Response>());
    }
    // Wait for majority to respond
    auto result =
        Await(Quorum(std::move(prepares), /*threshold=*/NodeCount() / 2 + 1))
            .ValueOrThrow();

    // Count number of ack==True
    uint32_t ack_count{0};
    // vote with highest ProposalNumber (if exists)
    std::optional<Proposal> highest;
    // Highest advice (if exists)
    ProposalNumber advice = ProposalNumber::Zero();
    for (auto& resp : result) {
      if (resp.ack) {
        ++ack_count;
        if (resp.vote.has_value()) {
          if (!highest.has_value() || highest->n < resp.vote->n) {
            highest = resp.vote;
          }
        }
      } else {
        if (advice < resp.advice) {
          advice = resp.advice;
        }
      }
    }
    uint64_t current_num;
    // set cum_ to advice (or higher)
    while ((current_num = num_.load()) < advice.k) {
      if (num_.compare_exchange_strong(current_num, advice.k)) {
        break;
      }
    }
    // If majority ack == True go to phase 2
    if (ack_count == result.size()) {
      std::optional<Value> answer;
      // if vote
      if (highest.has_value()) {
        answer = Phase2(highest->value, n, log_index);
        // if no vote
      } else {
        answer = Phase2(input, n, log_index);
      }
      if (answer.has_value()) {
        return answer.value();
      }
    }
    Future<void> timer = node::rt::After(backoff.Next());
    await::fibers::Await(std::move(timer)).ExpectOk();
  }
}

std::optional<Value> Proposer::Phase2(Value input, ProposalNumber n,
                                      size_t log_index) {
  proto::Accept::Request request{{n, input}, log_index};
  std::vector<Future<proto::Accept::Response>> accepts;

  for (const auto& peer : ListPeers().WithMe()) {
    accepts.push_back(commute::rpc::Call("Acceptor.Accept")
                          .Args(request)
                          .Via(Channel(peer))
                          .Start()
                          .As<proto::Accept::Response>());
  }
  auto result =
      Await(Quorum(std::move(accepts), /*threshold=*/NodeCount() / 2 + 1))
          .ValueOrThrow();

  uint32_t ack_count{0};
  std::optional<Proposal> highest;
  ProposalNumber advice = ProposalNumber::Zero();
  for (auto& resp : result) {
    if (resp.ack) {
      ++ack_count;
    } else {
      if (advice < resp.advice) {
        advice = resp.advice;
      }
    }
  }
  if (ack_count == result.size()) {
    return input;
  }
  uint64_t current_num;
  while ((current_num = num_.load()) < advice.k) {
    if (num_.compare_exchange_strong(current_num, advice.k)) {
      break;
    }
  }
  return std::nullopt;
}

}  // namespace paxos
