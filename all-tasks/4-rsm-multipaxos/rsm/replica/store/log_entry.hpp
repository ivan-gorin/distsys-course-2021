#pragma once

#include <rsm/client/command.hpp>
#include <rsm/replica/paxos/proposal.hpp>

#include <muesli/serializable.hpp>

namespace rsm {

struct LogEntry {
  // Acceptor state?
  bool is_commited{false};
  paxos::ProposalNumber prepare{};
  std::optional<paxos::Proposal> proposal = std::nullopt;
  std::optional<Command> command = std::nullopt;

  // Make empty log entry
  static LogEntry Empty() {
    return {};
  }

  MUESLI_SERIALIZABLE(is_commited, prepare, proposal, command)
};

}  // namespace rsm
