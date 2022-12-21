#include <rsm/proxy/client.hpp>

#include <rsm/client/command.hpp>
#include <rsm/replica/response.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

#include <commute/rpc/call.hpp>
#include <commute/rpc/client.hpp>
#include <commute/rpc/errors.hpp>

using namespace whirl;

namespace rsm {

ProxyClient::ProxyClient(const std::string& rsm_pool_name)
    : logger_("RSM-Proxy-Client", node::rt::LoggerBackend()) {
  ConnectToRSM(rsm_pool_name);
}

muesli::Bytes ProxyClient::Execute(const Command& command) {
  size_t attempt = 0;

  while (true) {
    ++attempt;

    std::string target_replica = ChooseReplica(command);

    LOG_INFO("Send command {} to {} (attempt {})", command.type, target_replica,
             attempt);

    auto f = commute::rpc::Call("RSM.Execute")
                 .Args(command)
                 .Via(Channel(target_replica))
                 .AtMostOnce()
                 .Context(await::context::ThisFiber())
                 .Start()
                 .As<Response>();

    auto result = await::fibers::Await(std::move(f));

    if (!result.IsOk()) {
      // RPC failed

      std::error_code error = result.GetError().GetErrorCode();

      if (IsRetriable(error)) {
        ForgetLeader();
        continue;  // Retry on transport errors
      } else {
        WHEELS_PANIC("Internal error, command "
                     << command.type << " failed: " << error.message());
      }
    }

    Response rsm_response = *result;

    if (rsm_response.index() == 0) {
      // Ok
      auto ok = std::get<0>(rsm_response);
      return ok.response;
    } else if (rsm_response.index() == 1) {
      // Redirect to leader
      RedirectToLeader redirect = std::get<1>(rsm_response);
      CacheLeader(redirect.host);
      LOG_INFO("Command {} redirected to leader {}", command.type,
               redirect.host);
      continue;
    } else if (rsm_response.index() == 2) {
      // Not a leader
      LOG_INFO("{} is not a leader, switch to another replica", target_replica);
      ForgetLeader();
      node::rt::SleepFor(50_jfs);
      continue;
    }
  }
}

void ProxyClient::CacheLeader(const std::string& host) {
  auto lock = mutex_.Guard();
  leader_.emplace(host);
}

void ProxyClient::ForgetLeader() {
  auto lock = mutex_.Guard();
  leader_.reset();
}

std::string ProxyClient::PickRandomReplica() {
  return replicas_[node::rt::RandomIndex(replicas_.size())];
}

std::string ProxyClient::ChooseReplica(const Command& /*cmd*/) {
  auto lock = mutex_.Guard();
  if (leader_.has_value()) {
    return *leader_;
  }
  return PickRandomReplica();
}

void ProxyClient::ConnectToRSM(const std::string& pool_name) {
  replicas_ = node::rt::Discovery()->ListPool(pool_name);

  auto client =
      commute::rpc::MakeClient(node::rt::NetTransport(), node::rt::Executor(),
                               node::rt::LoggerBackend());

  for (const auto& hostname : replicas_) {
    auto channel = client->Dial(hostname + ":42");
    channels_.emplace(hostname, channel);
  }
}

commute::rpc::IChannelPtr ProxyClient::Channel(const std::string& host) {
  return channels_[host];
}

bool ProxyClient::IsRetriable(const std::error_code& error) {
  return error == RPCErrorCode::TransportError;
}

}  // namespace rsm
