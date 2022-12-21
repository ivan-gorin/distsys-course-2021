#pragma once

#include "service.hpp"

#include <rpc/channel.hpp>
#include <rpc/runtime.hpp>

//////////////////////////////////////////////////////////////////////

rpc::IChannelPtr MakeFairLossChannel(ITestServicePtr service, size_t fails,
                                     rpc::IRuntime* runtime);
