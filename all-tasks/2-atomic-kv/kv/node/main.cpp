#include <kv/node/main.hpp>

// Node
#include <whirl/node/runtime/shortcuts.hpp>
#include <whirl/node/rpc/server.hpp>
#include <whirl/node/cluster/peer.hpp>
#include <whirl/node/store/kv.hpp>

// RPC
#include <commute/rpc/service_base.hpp>
#include <commute/rpc/call.hpp>

// Serialization
#include <muesli/serializable.hpp>
// Support std::string serialization
#include <cereal/types/string.hpp>

// Logging
#include <timber/log.hpp>

// Concurrency
#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/future.hpp>
#include <await/futures/combine/quorum.hpp>
#include <await/futures/util/never.hpp>
#include <await/fibers/sync/mutex.hpp>
#include <twist/stdlike/atomic.hpp>

#include <algorithm>

using await::fibers::Await;
using await::futures::Future;
using wheels::Result;

using namespace whirl;

//////////////////////////////////////////////////////////////////////

// Key -> Value
using Key = std::string;
using Value = std::string;

//////////////////////////////////////////////////////////////////////

struct WriteTimestamp {
  uint64_t value;
  int64_t nodeid;
  uint64_t local;

  static WriteTimestamp Min() {
    return {0, 0, 0};
  }

  bool operator<(const WriteTimestamp& that) const {
    if (value == that.value) {
      if (nodeid == that.nodeid) {
        return local < that.local;
      }
      return nodeid < that.nodeid;
    }
    return value < that.value;
  }

  // Serialization support (RPC, Database)
  MUESLI_SERIALIZABLE(value, nodeid, local)
};

// Logging support
std::ostream& operator<<(std::ostream& out, const WriteTimestamp& ts) {
  out << ts.value << ";" << ts.nodeid << ";" << ts.local;
  return out;
}

//////////////////////////////////////////////////////////////////////

// Replicas store versioned (stamped) values

struct StampedValue {
  Value value;
  WriteTimestamp timestamp;

  MUESLI_SERIALIZABLE(value, timestamp)
};

std::ostream& operator<<(std::ostream& out, const StampedValue& stamped_value) {
  out << "{" << stamped_value.value << ", ts: " << stamped_value.timestamp
      << "}";
  return out;
}

//////////////////////////////////////////////////////////////////////

// Coordinator role, stateless

class Coordinator : public commute::rpc::ServiceBase<Coordinator>,
                    public node::cluster::Peer {
 public:
  Coordinator()
      : Peer(node::rt::Config()),
        logger_("KVNode.Coordinator", node::rt::LoggerBackend()) {
  }

  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_METHOD(Set);
    COMMUTE_RPC_REGISTER_METHOD(Get);
  };

  // RPC handlers

  void Set(Key key, Value value) {
    WriteTimestamp write_ts = ChooseWriteTimestamp(key);
    write_ts.local = timestamp_.fetch_add(1);
    LOG_INFO("Write timestamp: {}", write_ts);

    std::vector<Future<void>> writes;

    // Broadcast LocalWrite
    for (const auto& peer : ListPeers().WithMe()) {
      writes.push_back(  //
          commute::rpc::Call("Replica.LocalWrite")
              .Args<Key, StampedValue>(key, {value, write_ts})
              .Via(Channel(peer))
              .Context(await::context::ThisFiber())
              .AtLeastOnce());
    }

    // Await acknowledgements from the majority of storage replicas
    Await(Quorum(std::move(writes), /*threshold=*/Majority())).ThrowIfError();
  }

  Value Get(Key key) {
    std::vector<Future<StampedValue>> reads;

    // Broadcast LocalRead
    for (const auto& peer : ListPeers().WithMe()) {
      reads.push_back(  //
          commute::rpc::Call("Replica.LocalRead")
              .Args(key)
              .Via(Channel(peer))
              .Context(await::context::ThisFiber())
              .AtLeastOnce());
    }

    auto stamped_values =
        Await(Quorum(std::move(reads), /*threshold=*/Majority()))
            .ValueOrThrow();

    auto most_recent = FindMostRecent(stamped_values);
    std::vector<Future<void>> writes;

    // Broadcast LocalWrite
    for (const auto& peer : ListPeers().WithMe()) {
      writes.push_back(  //
          commute::rpc::Call("Replica.LocalWrite")
              .Args<Key, StampedValue>(key, most_recent)
              .Via(Channel(peer))
              .Context(await::context::ThisFiber())
              .AtLeastOnce());
    }

    // Await acknowledgements from the majority of storage replicas
    Await(Quorum(std::move(writes), /*threshold=*/Majority())).ThrowIfError();

    return most_recent.value;
  }

 private:
  WriteTimestamp ChooseWriteTimestamp(Key key) const {
    std::vector<Future<StampedValue>> reads;

    // Broadcast LocalRead
    for (const auto& peer : ListPeers().WithMe()) {
      reads.push_back(  //
          commute::rpc::Call("Replica.LocalRead")
              .Args(key)
              .Via(Channel(peer))
              .Context(await::context::ThisFiber())
              .AtLeastOnce());
    }

    auto stamped_values =
        Await(Quorum(std::move(reads), /*threshold=*/Majority()))
            .ValueOrThrow();

    auto most_recent = FindMostRecent(stamped_values);
    return {most_recent.timestamp.value + 1,
            node::rt::Config()->GetInt64("node.id"), 0};
  }

  // Find value with the largest timestamp
  StampedValue FindMostRecent(const std::vector<StampedValue>& values) const {
    return *std::max_element(
        values.begin(), values.end(),
        [](const StampedValue& lhs, const StampedValue& rhs) {
          return lhs.timestamp < rhs.timestamp;
        });
  }

  // Quorum size
  size_t Majority() const {
    return NodeCount() / 2 + 1;
  }

 private:
  timber::Logger logger_;
  twist::stdlike::atomic<uint64_t> timestamp_{0};
};

//////////////////////////////////////////////////////////////////////

// Storage replica role

class Replica : public commute::rpc::ServiceBase<Replica> {
 public:
  Replica()
      : kv_store_(node::rt::Database(), "data"),
        logger_("KVNode.Replica", node::rt::LoggerBackend()) {
  }

  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_METHOD(LocalWrite);
    COMMUTE_RPC_REGISTER_METHOD(LocalRead);
  };

  // RPC handlers

  void LocalWrite(Key key, StampedValue target_value) {
    std::lock_guard<await::fibers::Mutex> lock(mutex_);
    std::optional<StampedValue> local_value = kv_store_.TryGet(key);

    if (!local_value.has_value()) {
      // First write for this key
      Update(key, target_value);
    } else {
      // Write timestamp > timestamp of locally stored value
      if (local_value->timestamp < target_value.timestamp) {
        Update(key, target_value);
      }
    }
  }

  StampedValue LocalRead(Key key) {
    return kv_store_.GetOr(key, {"", WriteTimestamp::Min()});
  }

 private:
  void Update(Key key, StampedValue target_value) {
    LOG_INFO("Write '{}' -> {}", key, target_value);
    kv_store_.Put(key, target_value);
  }

 private:
  // Local persistent K/V storage
  // strings -> StampedValues
  node::store::KVStore<StampedValue> kv_store_;

  timber::Logger logger_;
  await::fibers::Mutex mutex_;
};

//////////////////////////////////////////////////////////////////////

// Main routine

void KVNodeMain() {
  node::rt::PrintLine("Starting at {}", node::rt::WallTimeNow());

  // Open local database

  auto db_path = node::rt::Config()->GetString("db.path");
  node::rt::Database()->Open(db_path);

  // Start RPC server

  auto rpc_port = node::rt::Config()->GetInt<uint16_t>("rpc.port");
  auto rpc_server = node::rpc::MakeServer(rpc_port);

  rpc_server->RegisterService("KV", std::make_shared<Coordinator>());
  rpc_server->RegisterService("Replica", std::make_shared<Replica>());

  rpc_server->Start();

  // Serving ...

  await::futures::BlockForever();
}
