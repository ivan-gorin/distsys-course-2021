#include <kv/node/main.hpp>
#include <kv/client/client.hpp>

// Node
#include <whirl/node/runtime/shortcuts.hpp>

// Serialization
#include <muesli/serializable.hpp>
// Support std::string serialization
#include <cereal/types/string.hpp>

// Logging
#include <timber/log.hpp>

// Concurrency
#include <await/fibers/core/api.hpp>
#include <await/fibers/sync/future.hpp>

// Simulation
#include <matrix/facade/world.hpp>
#include <matrix/world/global/vars.hpp>
#include <matrix/world/global/time.hpp>
#include <matrix/client/rpc.hpp>
#include <matrix/client/util.hpp>
#include <matrix/test/random.hpp>
#include <matrix/test/main.hpp>
#include <matrix/test/event_log.hpp>
#include <matrix/test/runner.hpp>

#include <matrix/time_model/catalog/crazy.hpp>

#include <matrix/fault/access.hpp>
#include <matrix/fault/net/star.hpp>
#include <matrix/fault/util.hpp>

#include <matrix/semantics/printers/kv.hpp>
#include <matrix/semantics/checker/check.hpp>
#include <matrix/semantics/models/kv.hpp>

#include <commute/rpc/id.hpp>

#include <algorithm>

using namespace whirl;

//////////////////////////////////////////////////////////////////////

static const std::vector<kv::Key> kKeys({"a", "b", "c"});

kv::Key RandomKey() {
  return kKeys.at(node::rt::RandomNumber(matrix::GetGlobal<size_t>("keys")));
}

kv::Value RandomValue() {
  return std::to_string(node::rt::RandomNumber(1, 100));
}

//////////////////////////////////////////////////////////////////////

[[noreturn]] void Client() {
  await::fibers::self::SetName("main");

  node::rt::SleepFor(123_jfs);

  // + Random delay
  node::rt::SleepFor({node::rt::RandomNumber(50, 100)});

  timber::Logger logger_{"Client", node::rt::LoggerBackend()};

  kv::BlockingClient kv_store{matrix::client::MakeRpcChannel(
      /*pool_name=*/"kv", /*port=*/42)};

  for (size_t i = 1;; ++i) {
    kv::Key key = RandomKey();
    if (matrix::client::Either()) {
      kv::Value value = RandomValue();
      LOG_INFO("Execute Set({}, {})", key, value);
      kv_store.Set(key, value);
      LOG_INFO("Set completed");
    } else {
      LOG_INFO("Execute Get({})", key);
      [[maybe_unused]] kv::Value result = kv_store.Get(key);
      LOG_INFO("Get({}) -> {}", key, result);
    }

    matrix::GlobalCounter("requests").Increment();

    // Random pause
    node::rt::SleepFor(node::rt::RandomNumber(1, 100));
  }
}

//////////////////////////////////////////////////////////////////////

[[noreturn]] void NetAdversary() {
  timber::Logger logger_{"Net-Adversary", node::rt::LoggerBackend()};

  // List system nodes
  auto pool = node::rt::Discovery()->ListPool("kv");

  auto& net = matrix::fault::Network();

  while (true) {
    node::rt::SleepFor(node::rt::RandomNumber(10, 1000));

    size_t center = node::rt::RandomNumber(pool.size());

    LOG_INFO("Make star with center at {}", pool[center]);

    matrix::fault::MakeStar(pool, center);

    matrix::fault::RandomPause(100_jfs, 300_jfs);

    net.Heal();
  }
}

//////////////////////////////////////////////////////////////////////

void NodeAdversary() {
  timber::Logger logger_{"Node-Adversary", node::rt::LoggerBackend()};

  // List system nodes
  auto pool = node::rt::Discovery()->ListPool("kv");

  static const matrix::TimePoint kNoMoreFaults = 5000;

  while (matrix::GlobalNow() < kNoMoreFaults) {
    // Some random delay
    matrix::fault::RandomPause(200_jfs, 800_jfs);

    auto& victim = matrix::fault::RandomServer(pool);

    switch (node::rt::RandomNumber(3)) {
      case 0:
        // Reboot
        victim.FastReboot();
        break;
      case 1:
        // Freeze
        victim.Pause();
        matrix::fault::RandomPause(100_jfs, 300_jfs);
        victim.Resume();
        break;
      case 2:
        // Clocks
        victim.AdjustWallClock();
        break;
    }
  }
}

//////////////////////////////////////////////////////////////////////

void NodeReaper() {
  timber::Logger logger_{"Node-Reaper", node::rt::LoggerBackend()};

  // List system nodes
  auto pool = node::rt::Discovery()->ListPool("kv");

  // Bound on number of crashes
  size_t bound = (pool.size() - 1) / 2;

  // [0, bound]
  size_t crashes = node::rt::RandomNumber(0, bound);

  LOG_INFO("Crash budget: {}", crashes);

  for (size_t i = 0; i < crashes; ++i) {
    matrix::fault::RandomPause(300_jfs, 1000_jfs);

    auto& victim = matrix::fault::RandomServer(pool);

    if (victim.IsAlive()) {
      victim.Crash();
    }
  }
}

//////////////////////////////////////////////////////////////////////

// Sequential specification for KV storage
// Used by linearizability checker
using KVStoreModel = semantics::KVStoreModel<kv::Key, kv::Value>;

//////////////////////////////////////////////////////////////////////

// Seed -> simulation digest
// Deterministic
size_t RunSimulation(size_t seed) {
  auto& runner = matrix::TestRunner::Access();

  static const Jiffies kTimeLimit = 20000_jfs;
  static const size_t kRequestsThreshold = 7;

  runner.Verbose() << "Simulation seed: " << seed << std::endl;

  matrix::Random random{seed};

  // Randomize simulation parameters
  const size_t replicas = random.Get(3, 5);
  const size_t clients = random.Get(2, 3);
  const size_t keys = random.Get(1, 2);

  runner.Verbose() << "Parameters: "
                   << "replicas = " << replicas << ", "
                   << "clients = " << clients << ", "
                   << "keys = " << keys << std::endl;

  // Reset RPC ids
  commute::rpc::ResetIds();

  matrix::facade::World world{seed};

  // Time model
  world.SetTimeModel(matrix::MakeCrazyTimeModel());

  // Cluster
  world.MakePool("kv", KVNodeMain).Size(replicas);

  // Clients
  world.AddClients(Client, /*count=*/clients);

  // Adversaries

  if (random.Maybe(3)) {
    if (random.Maybe(3)) {
      // Network partitions
      world.AddAdversary(NetAdversary);
    }

    if (random.Maybe(7)) {
      // Reboots, pauses
      world.AddAdversary(NodeAdversary);
    }

    if (random.Maybe(11)) {
      // Crashes
      world.AddAdversary(NodeReaper);
    }
  }

  // Log file

  auto log_fpath = runner.LogFile();
  if (log_fpath) {
    world.WriteLogTo(*log_fpath);
  }
  if (auto path = runner.TraceFile()) {
    world.WriteTraceTo(*path);
  }

  // Globals
  world.SetGlobal("keys", keys);
  world.InitCounter("requests", 0);

  // Run simulation

  world.Start();
  while (world.GetCounter("requests") < kRequestsThreshold &&
         world.TimeElapsed() < kTimeLimit) {
    if (!world.Step()) {
      break;  // Deadlock
    }
  }

  // Stop and compute simulation digest
  size_t digest = world.Stop();

  // Print report
  runner.Verbose() << "Seed " << seed << " -> "
                   << "digest: " << digest << ", time: " << world.TimeElapsed()
                   << ", steps: " << world.StepCount() << std::endl;

  const auto event_log = world.EventLog();

  runner.Verbose() << "Requests completed: " << world.GetCounter("requests")
                  << std::endl;

  // Time limit exceeded
  if (world.GetCounter("requests") < kRequestsThreshold) {
    // Log
    runner.Report() << "Log:" << std::endl;
    matrix::WriteTextLog(event_log, runner.Report());
    runner.Report() << std::endl;

    if (world.TimeElapsed() < kTimeLimit) {
      runner.Report() << "Deadlock in simulation" << std::endl;
    } else {
      runner.Report() << "Simulation time limit exceeded" << std::endl;
    }
    runner.Fail();
  }

  // Check linearizability
  const auto history = world.History();
  const bool linearizable = semantics::LinCheck<KVStoreModel>(history);

  if (!linearizable) {
    // Log
    runner.Verbose() << "Log:" << std::endl;
    matrix::WriteTextLog(event_log, runner.Verbose());
    runner.Verbose() << std::endl;

    // History
    runner.Report() << "History is NOT LINEARIZABLE for seed = " << seed << ":"
                    << std::endl;
    semantics::PrintKVHistory<kv::Key, kv::Value>(history, runner.Report());

    runner.Fail();
  }

  return digest;
}

// Usage:
// 1) --det --sims 12345 - check determinism and run 12345 simulations
// 2) --seed 54321 - run single simulation with seed 54321

int main(int argc, const char** argv) {
  return matrix::Main(argc, argv, RunSimulation);
}
