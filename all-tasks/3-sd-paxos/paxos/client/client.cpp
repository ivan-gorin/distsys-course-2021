#include <paxos/client/client.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

#include <commute/rpc/call.hpp>

#include <await/fibers/sync/future.hpp>
#include <await/fibers/core/await.hpp>

#include <fmt/core.h>

namespace paxos {

BlockingClient::Value BlockingClient::Propose(Value value) {
  auto f = commute::rpc::Call("Proposer.Propose")
      .Args(value).Via(channel_)
      .TraceWith(GenerateTraceId(value))
      .AtLeastOnce()
      .Start()
      .As<Value>();

  return await::fibers::Await(std::move(f)).ValueOrThrow();
}

std::string BlockingClient::GenerateTraceId(const Value& value) {
  return fmt::format("Propose-{}-{}", value, whirl::node::rt::GenerateGuid());
}

}  // namespace paxos
