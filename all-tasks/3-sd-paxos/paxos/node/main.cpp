#include <paxos/node/main.hpp>

#include <paxos/node/proposer.hpp>
#include <paxos/node/acceptor.hpp>
#include <paxos/node/learner.hpp>

#include <whirl/node/rpc/server.hpp>
#include <whirl/node/runtime/shortcuts.hpp>

#include <await/futures/util/never.hpp>

using namespace whirl;

namespace paxos {

void NodeMain() {
  node::rt::PrintLine("Starting at {}", node::rt::WallTimeNow());

  // Open local database

  auto db_path = node::rt::Config()->GetString("db.path");
  node::rt::Database()->Open(db_path);

  // Start RPC server

  auto rpc_port = node::rt::Config()->GetInt<uint16_t>("rpc.port");
  auto rpc_server = node::rpc::MakeServer(rpc_port);

  rpc_server->RegisterService("Proposer", std::make_shared<Proposer>());
  rpc_server->RegisterService("Acceptor", std::make_shared<Acceptor>());

  rpc_server->Start();

  // Serving ...

  await::futures::BlockForever();
}

}  // namespace paxos
