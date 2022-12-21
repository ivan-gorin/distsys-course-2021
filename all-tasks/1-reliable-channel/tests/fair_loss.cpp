#include "fair_loss.hpp"

#include <rpc/channel.hpp>
#include <rpc/runtime.hpp>
#include <rpc/errors.hpp>

#include <await/futures/util/wrap.hpp>

#include <timber/log.hpp>

#include <atomic>

using await::futures::Future;
using wheels::Result;

using namespace std::chrono_literals;

//////////////////////////////////////////////////////////////////////

class FairLoss {
 public:
  explicit FairLoss(size_t fails) : fails_(fails), fails_left_(fails) {
  }

  bool Test() {
    while (true) {
      size_t curr = fails_left_.load();
      if (curr > 0) {
        if (fails_left_.compare_exchange_strong(curr, curr - 1)) {
          return false;
        }
      } else {
        // curr == 0
        if (fails_left_.compare_exchange_strong(curr, fails_)) {
          return true;
        }
      }
    }
  }

 private:
  const size_t fails_;
  std::atomic<size_t> fails_left_;
};

//////////////////////////////////////////////////////////////////////

class TestChannel : public rpc::IChannel {
 public:
  TestChannel(ITestServicePtr service, size_t fails, rpc::IRuntime* runtime)
      : service_(std::move(service)), runtime_(runtime), tester_(fails),
        logger_("Fair-Loss", runtime->Log()) {
  }

  Future<rpc::Message> Call(rpc::Method method, rpc::Message request,
                           rpc::CallOptions /*options*/) override {
    return await::futures::WrapResult(
        DoCall(std::move(method), std::move(request)));
  }

  ~TestChannel() {
    service_ = nullptr;
    runtime_ = nullptr;
  }

 private:
  Result<rpc::Message> DoCall(rpc::Method method, rpc::Message request) {
    if (!tester_.Test()) {
      LOG_INFO("Call {}.{} failed", service_->Name(), method);
      return TransportError();
    } else {
      LOG_INFO("Call {}.{} succeeded", service_->Name(), method);
      return wheels::make_result::Ok(service_->Call(method, request));
    }
  }

  static Result<rpc::Message> TransportError() {
    return wheels::make_result::Fail(rpc::TransportError());
  }

 private:
  ITestServicePtr service_;
  [[maybe_unused]] rpc::IRuntime* runtime_;
  FairLoss tester_;
  timber::Logger logger_;
};

//////////////////////////////////////////////////////////////////////

rpc::IChannelPtr MakeFairLossChannel(ITestServicePtr service, size_t fails,
                                     rpc::IRuntime* runtime) {
  return std::make_shared<TestChannel>(std::move(service), fails, runtime);
}
