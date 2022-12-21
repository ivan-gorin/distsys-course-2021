#pragma once

#include <rsm/client/command.hpp>

#include <muesli/serializable.hpp>

// Enable string serialization
#include <cereal/types/string.hpp>

#include <string>
#include <ostream>

namespace paxos {

////////////////////////////////////////////////////////////////////////////////

using Value = rsm::Command;

////////////////////////////////////////////////////////////////////////////////

// Proposal number

struct ProposalNumber {
  uint64_t k = 0;
  uint64_t node_id = 0;

  static ProposalNumber Zero() {
    return {0, 0};
  }

  bool operator<(const ProposalNumber& that) const {
    if (k == that.k) {
      return node_id < that.node_id;
    }
    return k < that.k;
  }

  bool operator<=(const ProposalNumber& that) const {
    if (k == that.k) {
      return node_id <= that.node_id;
    }
    return k <= that.k;
  }

  void Increment() {
    ++k;
  }

  MUESLI_SERIALIZABLE(k, node_id)
};

inline std::ostream& operator<<(std::ostream& out, const ProposalNumber& n) {
  out << "{" << n.k << " " << n.node_id << "}";
  return out;
}

////////////////////////////////////////////////////////////////////////////////

// Proposal = Proposal number + Value

struct Proposal {
  ProposalNumber n;
  Value value;

  MUESLI_SERIALIZABLE(n, value)
};

inline std::ostream& operator<<(std::ostream& out, const Proposal& proposal) {
  out << "{" << proposal.n << ", " << proposal.value << "}";
  return out;
}

}  // namespace paxos
