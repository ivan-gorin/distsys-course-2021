#pragma once

#include <muesli/serializable.hpp>
#include <cereal/types/string.hpp>

#include <string>
#include <ostream>
#include <tuple>

namespace rsm {

//////////////////////////////////////////////////////////////////////

struct RequestId {
  std::string client_id;
  uint64_t index;

  MUESLI_SERIALIZABLE(client_id, index)
};

//////////////////////////////////////////////////////////////////////

inline bool operator==(const RequestId& lhs, const RequestId& rhs) {
  return lhs.client_id == rhs.client_id && lhs.index == rhs.index;
}

inline bool operator!=(const RequestId& lhs, const RequestId& rhs) {
  return !(lhs == rhs);
}

inline bool operator<(const RequestId& lhs, const RequestId& rhs) {
  return std::tie(lhs.client_id, lhs.index) <
         std::tie(rhs.client_id, rhs.index);
}

//////////////////////////////////////////////////////////////////////

inline std::ostream& operator<<(std::ostream& out, const RequestId id) {
  out << "client-" << id.client_id << "-idx-" << id.index;
  return out;
}

}  // namespace rsm
