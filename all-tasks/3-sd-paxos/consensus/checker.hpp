#pragma once

#include <matrix/semantics/history.hpp>

namespace consensus {

// Checks safety properties of consensus
bool IsSafe(const whirl::semantics::History& history);

}   // namespace consensus
