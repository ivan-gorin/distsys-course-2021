#pragma once

#include <runtime/matrix/clock.hpp>

#include <timber/backend.hpp>

namespace runtime::matrix {

class LogBackend : public timber::ILogBackend {
 public:
  explicit LogBackend(Clock& clock) : clock_(clock) {
  }

  timber::Level GetMinLevelFor(
      const std::string& /*component*/) const override {
    return timber::Level::All;
  }

  void Log(timber::Event event) override;

 private:
  Clock& clock_;
};

}  // namespace runtime::matrix
