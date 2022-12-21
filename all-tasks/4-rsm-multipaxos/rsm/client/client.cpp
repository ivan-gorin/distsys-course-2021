#include <rsm/client/client.hpp>

#include <rsm/replica/response.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

#include <whirl/node/rpc/random.hpp>

#include <commute/rpc/call.hpp>
#include <commute/rpc/client.hpp>
#include <commute/rpc/errors.hpp>

using namespace whirl;

namespace rsm {

Client::Client(commute::rpc::IChannelPtr proxies)
    : proxies_(std::move(proxies)) {
  GenerateClientId();
}

muesli::Bytes Client::Execute(std::string type, muesli::Bytes request,
                              bool readonly) {
  auto request_id = NextRequestId();

  Command cmd{std::move(type), std::move(request), request_id, readonly};

  auto f = commute::rpc::Call("RSM.Execute")
               .Args(cmd)
               .Via(proxies_)
               .TraceWith(MakeTraceId(cmd))
               .AtLeastOnce()
               .Start()
               .As<muesli::Bytes>();

  return await::fibers::Await(std::move(f)).ValueOrThrow();
}

void Client::GenerateClientId() {
  client_id_ = node::rt::GenerateGuid();
}

commute::rpc::TraceId Client::MakeTraceId(const Command& cmd) {
  return fmt::format("{}", cmd.request_id);
}

RequestId Client::NextRequestId() {
  return {client_id_, ++request_index_};
}

}  // namespace rsm