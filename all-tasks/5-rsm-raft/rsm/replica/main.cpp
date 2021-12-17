#include <rsm/replica/main.hpp>

#include <rsm/replica/raft.hpp>
#include <rsm/replica/service.hpp>

#include <whirl/node/rpc/server.hpp>
#include <whirl/node/runtime/shortcuts.hpp>

#include <await/futures/util/never.hpp>

using namespace whirl;

namespace rsm {

void ReplicaMain(IStateMachinePtr state_machine) {
  node::rt::Database()->Open(node::rt::Config()->GetString("db.path"));

  auto rpc_server = whirl::node::rpc::MakeServer(
      node::rt::Config()->GetInt<uint16_t>("rpc.port"));

  auto replica =
      rsm::MakeRaftReplica(std::move(state_machine), rpc_server.get());

  auto service = std::make_shared<rsm::ReplicaService>(replica);

  rpc_server->RegisterService("RSM", service);

  rpc_server->Start();

  await::futures::BlockForever();
}

}  // namespace rsm
