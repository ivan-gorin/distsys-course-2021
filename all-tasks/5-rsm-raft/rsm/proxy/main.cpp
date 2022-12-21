#include <rsm/proxy/main.hpp>

#include <rsm/proxy/service.hpp>

#include <whirl/node/runtime/shortcuts.hpp>
#include <whirl/node/rpc/server.hpp>

#include <await/futures/util/never.hpp>

using namespace whirl;

namespace rsm {

std::string RsmPoolName() {
  return node::rt::Config()->GetString("rsm.pool.name");
}

commute::rpc::IServicePtr MakeProxyService() {
  return std::make_shared<ProxyService>(RsmPoolName());
}

void ProxyMain() {
  auto rpc_server = node::rpc::MakeServer(  //
      node::rt::Config()->GetInt<uint16_t>("rpc.port"));

  rpc_server->RegisterService("RSM-Proxy", MakeProxyService());
  rpc_server->Start();

  await::futures::BlockForever();
}

}  // namespace rsm
