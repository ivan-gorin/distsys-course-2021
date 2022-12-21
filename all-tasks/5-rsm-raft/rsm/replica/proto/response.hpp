#pragma once

#include <muesli/bytes.hpp>
#include <muesli/empty.hpp>
#include <muesli/serializable.hpp>

#include <string>
#include <variant>

#include <cereal/types/string.hpp>
#include <cereal/types/variant.hpp>

namespace rsm {

namespace proto {

// Replica response

//////////////////////////////////////////////////////////////////////

struct Ack {
  // Serialized operation response
  muesli::Bytes response;

  MUESLI_SERIALIZABLE(response);
};

//////////////////////////////////////////////////////////////////////

struct RedirectToLeader {
  std::string host;

  MUESLI_SERIALIZABLE(host);
};

//////////////////////////////////////////////////////////////////////

using NotALeader = muesli::EmptyMessage;

//////////////////////////////////////////////////////////////////////

using Response = std::variant<Ack, RedirectToLeader, NotALeader>;

}  // namespace proto

}  // namespace rsm
