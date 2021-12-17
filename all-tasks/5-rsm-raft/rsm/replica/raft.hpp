#pragma once

#include <rsm/replica/replica.hpp>
#include <rsm/replica/state_machine.hpp>

#include <commute/rpc/server.hpp>

namespace rsm {

IReplicaPtr MakeRaftReplica(IStateMachinePtr state_machine,
                            commute::rpc::IServer* server);

}  // namespace rsm
