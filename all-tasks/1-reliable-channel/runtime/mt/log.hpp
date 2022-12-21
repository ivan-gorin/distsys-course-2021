#pragma once

#include <timber/backend.hpp>

#include <iostream>
#include <mutex>

namespace runtime::mt {

class LogBackend : public timber::ILogBackend {
 public:
  timber::Level GetMinLevelFor(
      const std::string& /*component*/) const override {
    return timber::Level::All;
  }

  void Log(timber::Event event) override;

 private:
  std::mutex log_mutex_;
};

}  // namespace runtime::mt
