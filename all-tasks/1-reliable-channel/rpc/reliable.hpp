#pragma once

#include <rpc/backoff.hpp>
#include <rpc/channel.hpp>
#include <rpc/runtime.hpp>

namespace rpc {

IChannelPtr MakeReliableChannel(IChannelPtr fair_loss,
                                Backoff::Params backoff_params,
                                IRuntime* runtime);

}  // namespace rpc
