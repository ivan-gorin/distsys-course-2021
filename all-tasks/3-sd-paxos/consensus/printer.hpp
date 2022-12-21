#pragma once

#include <consensus/value.hpp>

#include <matrix/semantics/history.hpp>

#include <sstream>

namespace consensus {

struct Printer {
  static std::string Print(const whirl::semantics::Call& call) {
    std::stringstream out;

    auto [input_value] = call.arguments.As<Value>();
    out << "Propose(" << input_value << ")";
    if (call.IsCompleted()) {
      out << ": " << call.result->As<Value>();
    } else {
      out << "?";
    }

    return out.str();
  }
};

}  // namespace consensus
