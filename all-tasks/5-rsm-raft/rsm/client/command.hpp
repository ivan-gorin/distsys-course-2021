#pragma once

#include <rsm/client/request_id.hpp>

#include <muesli/bytes.hpp>
#include <muesli/serializable.hpp>
#include <cereal/types/string.hpp>

#include <string>
#include <ostream>

namespace rsm {

//////////////////////////////////////////////////////////////////////

struct Command {
  // Operation type (Set/Get/Cas)
  std::string type;

  // Serialized operation request
  muesli::Bytes request;

  // Globally unique request id
  // for implementing exactly-once semantics
  RequestId request_id;

  // Command metadata
  bool readonly;

  MUESLI_SERIALIZABLE(type, request, request_id, readonly);
};

//////////////////////////////////////////////////////////////////////

inline bool operator==(const Command& lhs, const Command& rhs) {
  return lhs.request_id == rhs.request_id;
}

inline bool operator!=(const Command& lhs, const Command& rhs) {
  return !(lhs == rhs);
}

//////////////////////////////////////////////////////////////////////

inline std::ostream& operator<<(std::ostream& out, const Command& cmd) {
  out << cmd.type << "--" << cmd.request_id;
  return out;
}

}  // namespace rsm
