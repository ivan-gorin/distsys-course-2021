#include <rpc/reliable.hpp>
#include <rpc/errors.hpp>

// Futures
#include <await/futures/core/future.hpp>

// Fibers
#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/future.hpp>

// Logging
#include <timber/log.hpp>

#include <wheels/support/compiler.hpp>

#include <chrono>

using await::fibers::Await;
using await::futures::Future;
using wheels::Result;

namespace rpc {

//////////////////////////////////////////////////////////////////

class ReliableChannel
    : public IChannel,
      public std::enable_shared_from_this<rpc::ReliableChannel> {
 public:
  ReliableChannel(IChannelPtr fair_loss, Backoff::Params backoff_params,
                  IRuntime* runtime)
      : fair_loss_(std::move(fair_loss)),
        backoff_params_(backoff_params),
        runtime_(runtime),
        logger_("Reliable", runtime->Log()) {
  }

  Future<Message> Call(Method method, Message request,
                       CallOptions options) override {
    WHEELS_UNUSED(backoff_params_);
    WHEELS_UNUSED(runtime_);

    LOG_INFO("Call({}, {}) started", method, request);
    auto [future, promise] = await::futures::MakeContract<Message>();

    await::fibers::Go([=, prom = std::move(promise),
                       self{shared_from_this()}]() mutable {
      auto backoff = backoff_params_.init;
      timber::Logger logger{"Reliable fiber", runtime_->Log()};
      LOG_INFO("Starting loop");
      while (true) {
        auto result =
            await::fibers::Await(fair_loss_->Call(method, request, options));
        if (result.HasValue()) {
          std::move(prom).SetValue(result.ValueOrThrow());
          return;
        }
        await::fibers::Await(runtime_->Timers()->After(backoff)).ExpectOk();
        if (options.stop_advice.StopRequested()) {
          LOG_INFO("Stop requested");
          std::move(prom).SetError(Cancelled());
          return;
        }
        backoff =
            std::min(backoff_params_.max, backoff * backoff_params_.factor);
      }
    });
    return std::move(future);
  }

 private:
  IChannelPtr fair_loss_;
  const Backoff::Params backoff_params_;
  IRuntime* runtime_;
  timber::Logger logger_;
};

//////////////////////////////////////////////////////////////////////

IChannelPtr MakeReliableChannel(IChannelPtr fair_loss,
                                Backoff::Params backoff_params,
                                IRuntime* runtime) {
  return std::make_shared<ReliableChannel>(std::move(fair_loss), backoff_params,
                                           runtime);
}

}  // namespace rpc
