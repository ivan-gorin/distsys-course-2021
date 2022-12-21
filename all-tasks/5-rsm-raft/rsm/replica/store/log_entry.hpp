#pragma once

#include <rsm/client/command.hpp>

#include <muesli/serializable.hpp>
#include <cereal/types/vector.hpp>

#include <vector>

namespace rsm {

struct LogEntry {
  Command command;
  uint64_t term;

  MUESLI_SERIALIZABLE(command, term)
};

using LogEntries = std::vector<LogEntry>;

}  // namespace rsm
