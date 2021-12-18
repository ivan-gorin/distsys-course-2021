#include <rsm/replica/raft.hpp>

#include <rsm/replica/proto/raft.hpp>
#include <rsm/replica/store/log.hpp>
#include <whirl/node/store/struct.hpp>

#include <commute/rpc/call.hpp>
#include <commute/rpc/service_base.hpp>

#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/channel.hpp>
#include <await/fibers/sync/mutex.hpp>
#include <await/fibers/sync/select.hpp>

#include <timber/log.hpp>

#include <whirl/node/runtime/shortcuts.hpp>
#include <whirl/node/cluster/peer.hpp>

#include <mutex>
#include <optional>
#include <algorithm>

using await::futures::Future;
using await::futures::Promise;

using namespace whirl;

namespace rsm {

enum class NodeState {
  Candidate = 1,  // Do not format
  Follower = 2,
  Leader = 3
};

class Raft : public IReplica,
             public commute::rpc::ServiceBase<Raft>,
             public node::cluster::Peer,
             public std::enable_shared_from_this<Raft> {
 public:
  Raft(IStateMachinePtr state_machine, persist::fs::Path store_dir)
      : Peer(node::rt::Config()),
        state_machine_(std::move(state_machine)),
        log_(node::rt::Fs(), store_dir),
        storage_(node::rt::Database(), "Persist"),
        logger_("Raft", node::rt::LoggerBackend()) {
  }

  Future<proto::Response> Execute(Command command) override {
    std::unique_lock guard(mutex_);
    auto [future, promise] = await::futures::MakeContract<proto::Response>();
    if (cache_.find(command.request_id) != cache_.end()) {
      std::move(promise).SetValue(proto::Ack{cache_[command.request_id]});
      return std::move(future);
    }
    if (state_ != NodeState::Leader) {
      // TODO: maybe only if follower
      if (leader_.has_value()) {
        std::move(promise).SetValue(proto::RedirectToLeader{leader_.value()});
      } else {
        std::move(promise).SetValue(proto::NotALeader{});
      }
      return std::move(future);
    }
    std::vector<LogEntry> new_entry{LogEntry{command, term_}};

    log_.Append(new_entry);
    LOG_INFO("Appended {} at term {}.", command, term_);
    PersistToStorage();
    trigger_ae_channel_.TrySend(1);
    WHEELS_ASSERT(
        commit_channels_.find(command.request_id) == commit_channels_.end(),
        "Command already in commit_channels_");
    commit_channels_.emplace(command.request_id, std::move(promise));

    return std::move(future);
  };

  void Start(commute::rpc::IServer* /*rpc_server*/) {
    std::unique_lock lock{mutex_};
    LOG_INFO("Starting...");
    state_machine_->Reset();
    log_.Open();
    auto st_voted_for = storage_.TryLoad<std::string>("votedFor");
    if (st_voted_for.has_value()) {
      voted_for_ = st_voted_for.value();
      persisted_voted_for_ = st_voted_for.value();
    }
    auto st_term = storage_.TryLoad<size_t>("currentTerm");
    if (st_term.has_value()) {
      term_ = st_term.value();
      persisted_term_ = term_;
    }
    auto st_commit_index = storage_.TryLoad<size_t>("commitIndex");
    if (st_commit_index.has_value()) {
      commit_index_ = st_commit_index.value();
      persisted_commit_index_ = commit_index_;
    }
    LOG_INFO("Filling cache, commit index {} log length {}", commit_index_,
             log_.Length());
    for (size_t i = 1; i <= commit_index_; ++i) {
      auto entry = log_.Read(i);
      if (cache_.find(entry.command.request_id) == cache_.end()) {
        auto response = state_machine_->Apply(entry.command);
        cache_[entry.command.request_id] = response;
      }
    }

    auto cur_term = term_;
    lock.unlock();

    await::fibers::Go([&, cur_term, self{shared_from_this()}]() {
      election_reset_event_ = node::rt::MonotonicNow();
      RunElectionTimer(cur_term);
    });
    LOG_INFO("Started");
  }

 protected:
  // RPC handlers

  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_HANDLER(RequestVote);
    COMMUTE_RPC_REGISTER_HANDLER(AppendEntries);
  }

  // Leader election

  void RequestVote(const raft::proto::RequestVote::Request& request,
                   raft::proto::RequestVote::Response* response) {
    std::lock_guard guard(mutex_);
    auto [last_log_index, last_log_term] = LastIndex();
    response->vote_granted = false;
    LOG_INFO(
        "RPC_RV: START term={}, candidate={}, index/term=({}, {}) "
        "[currentTerm={}, votedFor={}, log index/term=({}, {})]",
        request.term, request.candidate, request.last_log_index,
        request.last_log_term, term_,
        (voted_for_.has_value()) ? voted_for_.value() : "None", last_log_index,
        last_log_term);
    if (request.term > term_) {
      LOG_INFO("RPC_RV: term out of date");
      BecomeFollower(request.term);
    }
    if (request.term == term_ &&
        (!voted_for_.has_value() || voted_for_.value() == request.candidate) &&
        (request.last_log_term > last_log_term ||
         (request.last_log_term == last_log_term &&
          request.last_log_index >= last_log_index))) {
      response->vote_granted = true;
      voted_for_ = request.candidate;
      election_reset_event_ = node::rt::MonotonicNow();
    }
    response->term = term_;
    PersistToStorage();
    LOG_INFO("RPC_RV: response {} {}", response->term, response->vote_granted);
  }

  // Replication

  void AppendEntries(const raft::proto::AppendEntries::Request& request,
                     raft::proto::AppendEntries::Response* response) {
    std::lock_guard guard{mutex_};
    LOG_INFO(
        "RPC_AE: START from leader {}, term {}, prev idx/term ({}/{}), "
        "leader_commit {}",
        request.leader, request.term, request.prev_log_index,
        request.prev_log_term, request.leader_commit_index);
    if (request.term > term_) {
      LOG_INFO("RPC_AE: term out of date");
      BecomeFollower(request.term);
    }

    response->success = false;
    if (request.term == term_) {
      leader_ = request.leader;
      if (state_ != NodeState::Follower) {
        BecomeFollower(request.term);
      }
      election_reset_event_ = node::rt::MonotonicNow();

      if (request.prev_log_index == 0 ||
          (request.prev_log_index <= log_.Length() &&
           request.prev_log_term == log_.Term(request.prev_log_index))) {
        // no prev log index or prev log index inside local log + term matches
        response->success = true;

        size_t log_insert_index = request.prev_log_index + 1;
        size_t new_entries_index = 0;
        while (true) {
          if (log_insert_index > log_.Length() ||
              new_entries_index >= request.entries.size()) {
            break;
          }
          if (log_.Term(log_insert_index) !=
              request.entries[new_entries_index].term) {
            break;
          }
          ++log_insert_index;
          ++new_entries_index;
        }

        if (new_entries_index < request.entries.size()) {
          if (log_.Length() >= log_insert_index) {
            log_.TruncateSuffix(log_insert_index);
          }

          log_.Append(request.entries, new_entries_index);
        }

        size_t new_commit_index =
            std::min(request.leader_commit_index, log_.Length());
        if (new_commit_index > commit_index_) {
          ++commit_index_;
          for (; commit_index_ < new_commit_index; ++commit_index_) {
            auto command = log_.Read(commit_index_).command;
            if (cache_.find(command.request_id) == cache_.end()) {
              auto response1 = state_machine_->Apply(command);
              cache_[command.request_id] = response1;
            }
          }
          // TODO: make prettier
          {
            auto command = log_.Read(commit_index_).command;
            if (cache_.find(command.request_id) == cache_.end()) {
              auto response1 = state_machine_->Apply(command);
              cache_[command.request_id] = response1;
            }
          }
        }
      } else {
        LOG_INFO("RPC_AE: Failure");
        if (request.prev_log_index > log_.Length()) {
          // prev log index outside local log
          LOG_INFO("RPC_AE: prev log index outside local log");
          response->conflict_index = log_.Length() + 1;
          response->conflict_term = 0;
        } else {
          // inside local log but term doesn't match
          LOG_INFO("RPC_AE: inside local log but term doesn't match {}",
                   request.prev_log_index);
          response->conflict_term = log_.Term(request.prev_log_index);
          //          response->conflict_index = 1;
          for (size_t i = request.prev_log_index - 1; i > 0; --i) {
            if (log_.Term(i) != response->conflict_term) {
              response->conflict_index = i + 1;
              break;
            }
          }
        }
      }
    }
    response->term = term_;
    LOG_INFO("RPC_AE: Final response {} {} {} {}", response->term,
             response->success, response->conflict_index,
             response->conflict_term);
    PersistToStorage();
  }

 private:
  // State changes

  // With mutex
  void BecomeFollower(size_t term) {
    LOG_INFO("Became follower with term {}", term);

    for (auto it = commit_channels_.begin(); it != commit_channels_.end();) {
      LOG_INFO("NotALeader to {}", it->first);
      std::move(it->second).SetValue(proto::NotALeader{});
      commit_channels_.erase(it++);
    }

    state_ = NodeState::Follower;
    term_ = term;
    voted_for_.reset();

    await::fibers::Go([&, term, self{shared_from_this()}]() {
      RunElectionTimer(term);
    });
  }

  // With mutex
  void BecomeLeader() {
    state_ = NodeState::Leader;
    auto len = log_.Length();

    for (auto peer : ListPeers().WithoutMe()) {
      next_index_[peer] = len + 1;
      match_index_[peer] = 0;
    }
    LOG_INFO("Became leader with term {}", term_);
    await::fibers::Go([&, self{shared_from_this()}]() {
      std::unique_lock lock{mutex_};
      SendAEs(term_);
      lock.unlock();
      await::fibers::Channel<int> heartbeat_channel{1};
      await::fibers::Go(
          [&, heartbeat_channel, self{shared_from_this()}]() mutable {
            node::rt::SleepFor(500);
            heartbeat_channel.Send(123);
          });
      bool do_send;
      while (true) {
        do_send = false;
        auto value =
            await::fibers::Select(heartbeat_channel, trigger_ae_channel_);
        switch (value.index()) {
          case 0:
            // heartbeat
            do_send = true;
            await::fibers::Go(
                [&, heartbeat_channel, self{shared_from_this()}]() mutable {
                  node::rt::SleepFor(500);
                  heartbeat_channel.Send(123);
                });
          case 1:
            do_send = true;
        }
        if (do_send) {
          lock.lock();
          if (state_ != NodeState::Leader) {
            return;
          }
          SendAEs(term_);
          lock.unlock();
        }
      }
    });
  }

 private:
  // Fibers

  void RunElectionTimer(size_t term) {
    auto timeout = ElectionTimeout();
    Jiffies ticker_interval = 500;
    LOG_INFO("Election timer started, timeout {} at term {}", timeout, term);
    await::fibers::Channel<int> ticker_channel{1};

    await::fibers::Go([&, ticker_channel, ticker_interval,
                       self{shared_from_this()}]() mutable {
      node::rt::SleepFor(ticker_interval);
      ticker_channel.Send(123);
    });
    while (true) {
      ticker_channel.Receive();
      std::lock_guard guard{mutex_};
      if (state_ == NodeState::Leader) {
        LOG_INFO("in election timer, state == leader, leaving");
        return;
      }
      if (term != term_) {
        LOG_INFO("in election timer, term changed, leaving");
        return;
      }
      if (node::rt::MonotonicNow() - election_reset_event_ >= timeout) {
        LOG_INFO("Starting election");
        StartElection();
        return;
      }
      await::fibers::Go([&, ticker_channel, ticker_interval,
                         self{shared_from_this()}]() mutable {
        //        LOG_INFO("Ticker");
        node::rt::SleepFor(ticker_interval);
        //        LOG_INFO("Ticker woke up");
        ticker_channel.Send(123);
      });
    }
  }

 private:
  // Misc

  void StartElection() {
    // With mutex
    state_ = NodeState::Candidate;
    ++term_;
    auto saved_cur_term = term_;
    election_reset_event_ = node::rt::MonotonicNow();
    voted_for_ = node::rt::HostName();
    // TODO: maybe leader_.reset();
    PersistToStorage();
    LOG_INFO("became candidate for term {}.", saved_cur_term);
    std::shared_ptr<size_t> votes_received = std::make_shared<size_t>(1);
    for (auto peer : ListPeers().WithoutMe()) {
      await::fibers::Go([&, peer, saved_cur_term, votes_received,
                         self{shared_from_this()}]() {
        mutex_.Lock();
        auto [last_log_index, last_log_term] = LastIndex();
        mutex_.Unlock();
        raft::proto::RequestVote::Request request{
            saved_cur_term, node::rt::HostName(), last_log_index,
            last_log_term};
        auto result =
            await::fibers::Await(commute::rpc::Call("Raft.RequestVote")
                                     .Args(request)
                                     .Via(Peer::Channel(peer))
                                     .Start()
                                     .As<raft::proto::RequestVote::Response>());
        if (result.HasError()) {
          LOG_INFO("Error in RequestVoteResponse");
          return;
        }
        auto reply = result.ValueOrThrow();
        std::lock_guard guard{mutex_};
        if (state_ != NodeState::Candidate) {
          LOG_INFO("No longer in Candidate, exiting election");
          return;
        }
        if (saved_cur_term != term_) {
          LOG_INFO("Different term, exiting election");
          return;
        }
        if (reply.term > saved_cur_term) {
          LOG_INFO("Term out of date in RequestVoteReply, becoming follower");
          BecomeFollower(reply.term);
          return;
        } else if (reply.term == saved_cur_term) {
          if (reply.vote_granted) {
            ++(*votes_received);
            if ((*votes_received) * 2 >= NodeCount() + 1) {
              BecomeLeader();
              return;
            }
          }
        }
      });
    }
    await::fibers::Go([&, saved_cur_term, self{shared_from_this()}]() {
      RunElectionTimer(saved_cur_term);
    });
  }

  std::tuple<size_t, size_t> LastIndex() {
    return {log_.Length(), log_.LastLogTerm()};
  }

  void SendAEs(size_t saved_cur_term) {
    LOG_INFO("Sending AEs at term {}", saved_cur_term);
    for (auto peer : ListPeers().WithoutMe()) {
      await::fibers::Go([&, saved_cur_term, peer, self{shared_from_this()}]() {
        std::unique_lock lock1(mutex_);
        auto ni = next_index_[peer];
        auto prev_log_index = ni - 1;
        size_t prev_log_term = 0;
        if (prev_log_index > 0) {
          prev_log_term = log_.Term(prev_log_index);
        }
        LogEntries entries;
        for (auto index = ni; index <= log_.Length(); ++index) {
          entries.push_back(log_.Read(index));
        }
        raft::proto::AppendEntries::Request request{
            saved_cur_term, node::rt::HostName(), prev_log_index, prev_log_term,
            entries,        commit_index_};
        lock1.unlock();
        LOG_INFO("Sending AE to {}", peer);
        auto result = await::fibers::Await(
            commute::rpc::Call("Raft.AppendEntries")
                .Args(request)
                .Via(Peer::Channel(peer))
                .Start()
                .As<raft::proto::AppendEntries::Response>());
        if (result.HasError()) {
          LOG_INFO("Error in AppendEntriesResponse");
          return;
        }
        raft::proto::AppendEntries::Response reply = result.ValueOrThrow();
        lock1.lock();
        if (term_ != saved_cur_term) {
          LOG_INFO("other current term, exiting");
          return;
        }
        if (reply.term > saved_cur_term) {
          LOG_INFO("term out of date in heartbeat reply");
          BecomeFollower(reply.term);
          return;
        }
        if (state_ == NodeState::Leader && saved_cur_term == reply.term) {
          if (reply.success) {
            next_index_[peer] = ni + entries.size();
            match_index_[peer] = next_index_[peer] - 1;

            size_t saved_commit_index = commit_index_;
            for (auto i = commit_index_ + 1; i <= log_.Length(); ++i) {
              if (log_.Term(i) == term_) {
                size_t match_count = 1;
                for (auto peer_name : ListPeers().WithoutMe()) {
                  if (match_index_[peer_name] >= i) {
                    ++match_count;
                  }
                }
                if (match_count * 2 >= NodeCount() + 1) {
                  commit_index_ = i;
                }
              }
            }

            if (saved_commit_index != commit_index_) {
              ++saved_commit_index;
              for (; saved_commit_index <= commit_index_;
                   ++saved_commit_index) {
                LOG_INFO("Committing index {}", saved_commit_index);
                auto command = log_.Read(saved_commit_index).command;
                if (cache_.find(command.request_id) == cache_.end()) {
                  auto response = state_machine_->Apply(command);
                  cache_[command.request_id] = response;
                  auto ch_iter = commit_channels_.find(command.request_id);
                  if (ch_iter != commit_channels_.end()) {
                    std::move(ch_iter->second).SetValue(proto::Ack{response});
                    commit_channels_.erase(ch_iter);
                  }
                }
                WHEELS_ASSERT(commit_channels_.find(command.request_id) ==
                                  commit_channels_.end(),
                              "Command already in commit_channels_");
              }
            }
          } else {
            if (reply.conflict_term > 0) {
              size_t last_index_of_term = 0;
              for (size_t i = log_.Length(); i > 0; --i) {
                if (log_.Term(i) == reply.conflict_term) {
                  last_index_of_term = i;
                  break;
                }
              }
              if (last_index_of_term > 0) {
                next_index_[peer] = last_index_of_term + 1;
              } else {
                next_index_[peer] = reply.conflict_index;
              }
            } else {
              next_index_[peer] = reply.conflict_index;
            }
          }
        }
      });
    }
  }

 private:
  Jiffies ElectionTimeout() const {
    uint64_t rtt = node::rt::Config()->GetInt<uint64_t>("net.rtt");
    // Your code goes here
    return Jiffies{6 * rtt + node::rt::RandomNumber(100)};
  }

  void PersistToStorage() {
    LOG_INFO("Persisting");
    if (voted_for_.has_value()) {
      if (!persisted_voted_for_.has_value() ||
          persisted_voted_for_.value() != voted_for_.value()) {
        storage_.Store("votedFor", voted_for_.value());
        persisted_voted_for_ = voted_for_.value();
      }
    }
    if (persisted_term_ != term_) {
      storage_.Store("currentTerm", term_);
      persisted_term_ = term_;
    }
    if (persisted_commit_index_ != commit_index_) {
      storage_.Store("commitIndex", commit_index_);
      persisted_commit_index_ = commit_index_;
    }
  }

  void PleaseCompiler() {
    WHEELS_UNUSED(log_);
    WHEELS_UNUSED(term_);
    WHEELS_UNUSED(state_);
    WHEELS_UNUSED(voted_for_);
    WHEELS_UNUSED(next_index_);
    WHEELS_UNUSED(match_index_);
    WHEELS_UNUSED(commit_index_);
  }

 private:
  await::fibers::Mutex mutex_;

  IStateMachinePtr state_machine_;

  Log log_;
  node::store::StructStore storage_;

  size_t term_{0};
  size_t persisted_term_{0};
  NodeState state_;

  std::optional<std::string> leader_;
  std::optional<std::string> voted_for_;
  std::optional<std::string> persisted_voted_for_;

  // Peer -> next index id
  std::map<std::string, size_t> next_index_;
  // Peer -> match index id
  std::map<std::string, size_t> match_index_;

  // log index -> commit channel
  std::map<rsm::RequestId, await::futures::Promise<proto::Response>>
      commit_channels_;
  std::map<rsm::RequestId, muesli::Bytes> cache_;

  size_t commit_index_{0};
  size_t persisted_commit_index_{0};

  node::time::MonotonicTime election_reset_event_{0};

  await::fibers::Channel<int> trigger_ae_channel_{1};

  timber::Logger logger_;
};

//////////////////////////////////////////////////////////////////////

IReplicaPtr MakeRaftReplica(IStateMachinePtr state_machine,
                            commute::rpc::IServer* server) {
  auto store_dir =
      node::rt::Fs()->MakePath(node::rt::Config()->GetString("rsm.store.dir"));

  auto db_path = node::rt::Config()->GetString("db.path");
  node::rt::Database()->Open(db_path);

  auto replica =
      std::make_shared<Raft>(std::move(state_machine), std::move(store_dir));
  server->RegisterService("Raft", replica);

  replica->Start(server);
  return replica;
}

}  // namespace rsm
