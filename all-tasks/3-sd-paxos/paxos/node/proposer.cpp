#include <paxos/node/proposer.hpp>

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

Value Proposer::Propose(Value input) {
  return Phase1(input);
}

Value Proposer::Phase1(Value input) {
  paxos::Backoff::Params params{1, 10, 2};
  paxos::Backoff backoff(params);
  while (true) {
    ProposalNumber n{num_.fetch_add(1),
                     (uint64_t)node::rt::Config()->GetInt64("node.id")};
    proto::Prepare::Request request{n};
    std::vector<Future<proto::Prepare::Response>> prepares;

    for (const auto& peer : ListPeers().WithMe()) {
      prepares.push_back(commute::rpc::Call("Acceptor.Prepare")
                             .Args(request)
                             .Via(Channel(peer))
                             .Start()
                             .As<proto::Prepare::Response>());
    }
    auto result =
        Await(Quorum(std::move(prepares), /*threshold=*/NodeCount() / 2 + 1))
            .ValueOrThrow();

    uint32_t ack_count{0};
    std::optional<Proposal> highest;
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
    while ((current_num = num_.load()) < advice.k) {
      if (num_.compare_exchange_strong(current_num, advice.k)) {
        break;
      }
    }
    if (ack_count == result.size()) {
      std::optional<Value> answer;
      if (highest.has_value()) {
        answer = Phase2(highest->value, n);
      } else {
        answer = Phase2(input, n);
      }
      if (answer.has_value()) {
        return answer.value();
      }
    }
    Future<void> timer =
        node::GetRuntime().TimeService()->After(backoff.Next());
    await::fibers::Await(std::move(timer)).ExpectOk();
  }
}

std::optional<Value> Proposer::Phase2(Value input, ProposalNumber n) {
  paxos::Backoff::Params params{0, 10, 2};
  paxos::Backoff backoff(params);
  proto::Accept::Request request{n, input};
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
