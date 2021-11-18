#include <rsm/replica/multipaxos.hpp>
#include <rsm/replica/paxos/proposer.hpp>
#include <rsm/replica/paxos/acceptor.hpp>

#include <rsm/replica/store/log.hpp>

#include <commute/rpc/call.hpp>

#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/channel.hpp>
#include <await/fibers/sync/mutex.hpp>

#include <timber/log.hpp>

#include <whirl/node/runtime/shortcuts.hpp>
#include <whirl/node/cluster/peer.hpp>

#include <map>

using await::fibers::Channel;
using await::futures::Future;
using await::futures::Promise;

using namespace whirl;

namespace rsm {

//////////////////////////////////////////////////////////////////////

class MultiPaxos : public IReplica, public whirl::node::cluster::Peer {
 public:
  MultiPaxos(IStateMachinePtr state_machine, persist::fs::Path store_dir,
             commute::rpc::IServer* server)
      : Peer(node::rt::Config()),
        state_machine_(std::move(state_machine)),
        log_(store_dir),
        logger_("Replica", node::rt::LoggerBackend()) {
    Start(server);
  }

  Future<Response> Execute(Command command) override {
    auto guard = mutex_.Guard();
    if (cache_.find(command.request_id) != cache_.end()) {
      auto [future, promise] = await::futures::MakeContract<Response>();
      std::move(promise).SetValue(Ack{cache_[command.request_id]});
      return std::move(future);
    }
    if (node::rt::HostName() != leader_) {
      auto [future, promise] = await::futures::MakeContract<Response>();
      std::move(promise).SetValue(RedirectToLeader{leader_});
      return std::move(future);
    }
    while (true) {
      {
        std::lock_guard<await::fibers::Mutex> lock(log_mutex_);
        while (true) {
          auto entry = log_.Read(index_);
          if (!entry.has_value() || !entry->is_commited) {
            break;
          } else {
            LOG_INFO("Executing command {}", entry->command.value());
            auto response = state_machine_->Apply(entry->command.value());
            cache_[entry->command.value().request_id] = response;
            ++index_;
          }
        }
      }
      LOG_INFO("Proposing command {} on index {}", command, index_);
      auto f = commute::rpc::Call("Proposer.Propose")
                   .Args(command, index_)
                   .Via(LoopBack())
                   .AtLeastOnce()
                   .Start()
                   .As<Command>();
      auto res = await::fibers::Await(std::move(f)).ValueOrThrow();
      //      auto res = proposer_->Propose(command, index_);

      {
        std::lock_guard<await::fibers::Mutex> lock(log_mutex_);
        LogEntry entry;
        if (!log_.IsEmpty(index_)) {
          entry = log_.Read(index_).value();
        }
        entry.is_commited = true;
        entry.command = res;
        log_.Update(index_, entry);
      }
      ++index_;
      if (res == command) {
        break;
      }
      LOG_INFO("Executing command {}", res);
      auto response = state_machine_->Apply(res);
      cache_[res.request_id] = response;
    }

    auto [future, promise] = await::futures::MakeContract<Response>();
    LOG_INFO("Executing command {}", command);
    auto response = state_machine_->Apply(command);
    cache_[command.request_id] = response;
    std::move(promise).SetValue(Ack{response});

    return std::move(future);
  };

  void Start(commute::rpc::IServer* rpc_server) {
    // Reset state machine state
    state_machine_->Reset();
    index_ = 1;
    // Open log on disk
    log_.Open();
    {
      std::lock_guard<await::fibers::Mutex> lock(log_mutex_);

      while (true) {
        auto entry = log_.Read(index_);
        if (!entry.has_value() || !entry->is_commited) {
          break;
        } else {
          LOG_INFO("Executing command {}", entry->command.value());
          auto response = state_machine_->Apply(entry->command.value());
          cache_[entry->command.value().request_id] = response;
          ++index_;
        }
      }
    }

    // Launch pipeline fibers
    node::rt::Go([this]() mutable {
      // Heartbeat
      auto hostname = node::rt::HostName();
      (void)this;
      while (true) {
        for (const auto& peer : ListPeers().WithMe()) {
          (void)commute::rpc::Call("RSM.Heartbeat")
              .Args(hostname)
              .Via(Channel(peer))
              .Start();
        }
        node::rt::SleepFor(heartbeat_);
        ++leader_timeout_;
        if (leader_timeout_ >= 2) {
          leader_timeout_ = 0;
          leader_ = hostname;
        }
      }
    });

    // Register RPC services
    proposer_ = std::make_shared<paxos::Proposer>();
    acceptor_ = std::make_shared<paxos::Acceptor>(log_, log_mutex_);
    rpc_server->RegisterService("Proposer", proposer_);
    rpc_server->RegisterService("Acceptor", acceptor_);
  }

  void UpdateLeader(std::string& new_leader) override {
    if (new_leader > leader_) {
      leader_ = new_leader;
    } else if (new_leader == leader_) {
      leader_timeout_ = 0;
    }
  }

 private:
  // Replicated state
  IStateMachinePtr state_machine_;
  std::shared_ptr<paxos::Proposer> proposer_;
  std::shared_ptr<paxos::Acceptor> acceptor_;

  // Persistent log
  Log log_;
  size_t index_ = 1;

  Jiffies heartbeat_{10000};
  std::string leader_{node::rt::HostName()};
  size_t leader_timeout_{0};

  std::map<rsm::RequestId, muesli::Bytes> cache_;

  await::fibers::Mutex mutex_;
  await::fibers::Mutex log_mutex_;

  // Logging
  timber::Logger logger_;

  // paxos
};

//////////////////////////////////////////////////////////////////////

IReplicaPtr MakeMultiPaxosReplica(IStateMachinePtr state_machine,
                                  commute::rpc::IServer* server) {
  auto store_dir =
      node::rt::Fs()->MakePath(node::rt::Config()->GetString("rsm.store.dir"));

  return std::make_shared<MultiPaxos>(std::move(state_machine),
                                      std::move(store_dir), server);
}

}  // namespace rsm
