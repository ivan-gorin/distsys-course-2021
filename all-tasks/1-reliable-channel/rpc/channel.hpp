#pragma once

#include <rpc/message.hpp>
#include <rpc/method.hpp>

#include <await/futures/core/future.hpp>
#include <await/context/stop_token.hpp>

#include <memory>
#include <string>

namespace rpc {

struct CallOptions {
  // Cooperative cancellation
  await::context::StopToken stop_advice;
};

// Communication line between client and server

struct IChannel {
  virtual ~IChannel() = default;

  // Asynchronous request to remote service
  virtual await::futures::Future<Message> Call(Method method, Message request,
                                               CallOptions options) = 0;
};

using IChannelPtr = std::shared_ptr<IChannel>;

}  // namespace rpc
