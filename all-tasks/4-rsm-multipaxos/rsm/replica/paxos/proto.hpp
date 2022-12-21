#pragma once

#include <rsm/replica/paxos/proposal.hpp>

#include <muesli/serializable.hpp>

#include <cereal/types/optional.hpp>

#include <optional>

namespace paxos {

namespace proto {

////////////////////////////////////////////////////////////////////////////////

// Phase I

struct Prepare {
  // Prepare
  struct Request {
    ProposalNumber n;
    size_t log_index;
    MUESLI_SERIALIZABLE(n, log_index)
  };

  // Promise
  struct Response {
    bool ack{true};
    ProposalNumber advice{ProposalNumber::Zero()};
    std::optional<Proposal> vote;

    MUESLI_SERIALIZABLE(ack, advice, vote)
  };
};

////////////////////////////////////////////////////////////////////////////////

// Phase II

struct Accept {
  // Accept
  struct Request {
    Proposal proposal;
    size_t log_index;
    MUESLI_SERIALIZABLE(proposal, log_index)
  };

  // Accepted
  struct Response {
    bool ack = false;
    ProposalNumber advice;

    MUESLI_SERIALIZABLE(ack, advice)
  };
};

}  // namespace proto

}  // namespace paxos
