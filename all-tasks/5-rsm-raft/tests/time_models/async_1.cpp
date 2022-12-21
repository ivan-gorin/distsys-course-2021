#include <tests/time_models/async_1.hpp>

#include <matrix/world/global/random.hpp>

using namespace whirl::matrix;
using whirl::Jiffies;

namespace tests {

//////////////////////////////////////////////////////////////////////

class ServerTimeModel : public IServerTimeModel {
 public:
  // Clocks

  int InitClockDrift() override {
    if (GlobalRandomNumber() % 2 == 0) {
      // Faster
      return GlobalRandomNumber(10);
    } else {
      // Slower
      return -(int)GlobalRandomNumber(10);
    }
  }

  TimePoint ResetMonotonicClock() override {
    return GlobalRandomNumber(1, 100);
  }

  Jiffies InitWallClockOffset() override {
    return GlobalRandomNumber(1000);
  }

  // TrueTime

  Jiffies TrueTimeUncertainty() override {
    return GlobalRandomNumber(5, 500);
  }

  // Disk

  Jiffies DiskWrite(size_t /*bytes*/) override {
    return GlobalRandomNumber(10, 250);
  }

  Jiffies DiskRead(size_t /*bytes*/) override {
    return GlobalRandomNumber(10, 50);
  }

  // Database

  bool GetCacheMiss() override {
    return GlobalRandomNumber(3) == 0;
  }

  bool IteratorCacheMiss() override {
    return GlobalRandomNumber(11) == 0;
  }

  // Threads

  Jiffies ThreadPause() override {
    return GlobalRandomNumber(5, 50);
  }
};

//////////////////////////////////////////////////////////////////////

class TimeModel : public ITimeModel {
 public:
  void Initialize() override {
    slow_message_freq_ = GlobalRandomNumber(5, 7);
    slow_message_delay_ = GlobalRandomNumber(200, 500);
  }

  TimePoint GlobalStartTime() override {
    return GlobalRandomNumber(1, 200);
  }

  // Server

  IServerTimeModelPtr MakeServerModel(const std::string& /*host*/) override {
    return std::make_unique<ServerTimeModel>();
  }

  // Network

  virtual Jiffies EstimateRtt() const override {
    return 1000;  // Backward compatibility
  }

  Jiffies FlightTime(const net::IServer* /*start*/, const net::IServer* /*end*/,
                     const net::Packet& /*packet*/) override {
    if (GlobalRandomNumber() % slow_message_freq_ == 0) {
      return GlobalRandomNumber(10, slow_message_delay_);
    }
    return GlobalRandomNumber(30, 60);
  }

  commute::rpc::BackoffParams BackoffParams() override {
    return {50, 1000, 2};
  }

 private:
  size_t slow_message_freq_ = 0;
  size_t slow_message_delay_ = 0;
};

//////////////////////////////////////////////////////////////////////

ITimeModelPtr MakeAsyncTimeModel() {
  return std::make_unique<TimeModel>();
}

}  // namespace tests
