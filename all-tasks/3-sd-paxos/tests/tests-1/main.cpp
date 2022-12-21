#include <paxos/client/client.hpp>
#include <paxos/node/main.hpp>

#include <consensus/value.hpp>
#include <consensus/checker.hpp>
#include <consensus/printer.hpp>

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
#include <matrix/test/random.hpp>
#include <matrix/test/main.hpp>
#include <matrix/test/event_log.hpp>
#include <matrix/test/runner.hpp>

#include <matrix/fault/access.hpp>
#include <matrix/fault/net/split.hpp>
#include <matrix/fault/util.hpp>

#include <matrix/semantics/printers/print.hpp>

#include <commute/rpc/id.hpp>

#include <algorithm>

#include "time_model/paxos.hpp"

using namespace whirl;

//////////////////////////////////////////////////////////////////////

[[noreturn]] void Client() {
  await::fibers::self::SetName("main");

  node::rt::SleepFor(123_jfs);

  // + Random delay
  node::rt::SleepFor({node::rt::RandomNumber(50, 100)});

  timber::Logger logger_{"Client", node::rt::LoggerBackend()};

  auto channel = matrix::client::MakeRpcChannel(
      /*pool_name=*/"paxos", /*port=*/42, /*log_retries=*/false);

  paxos::BlockingClient paxos{channel};

  while (true) {
    consensus::Value value = std::to_string(node::rt::RandomNumber(100));
    LOG_INFO("Start Propose({})", value);
    auto chosen_value = paxos.Propose(value);
    LOG_INFO("Chosen value: {}", chosen_value);

    matrix::GlobalCounter("requests").Increment();

    // Random pause
    node::rt::SleepFor(node::rt::RandomNumber(1, 100));
  }
}

//////////////////////////////////////////////////////////////////////

static const matrix::TimePoint kNoMoreFaults = 10000;

//////////////////////////////////////////////////////////////////////

void NetAdversary() {
  timber::Logger logger_{"Net-Adversary", node::rt::LoggerBackend()};

  // List system nodes
  auto pool = node::rt::Discovery()->ListPool("paxos");

  auto& net = matrix::fault::Network();

  while (matrix::GlobalNow() < kNoMoreFaults) {
    node::rt::SleepFor(node::rt::RandomNumber(10, 1000));

    size_t lhs_size = node::rt::RandomNumber(1, pool.size() - 1);
    LOG_INFO("Random split: {}/{}", lhs_size, pool.size() - lhs_size);
    matrix::fault::RandomSplit(pool, lhs_size);

    matrix::fault::RandomPause(100_jfs, 500_jfs);

    net.Heal();
  }
}

//////////////////////////////////////////////////////////////////////

void NodeAdversary() {
  timber::Logger logger_{"Node-Adversary", node::rt::LoggerBackend()};

  // List system nodes
  auto pool = node::rt::Discovery()->ListPool("paxos");

  while (matrix::GlobalNow() < kNoMoreFaults) {
    // Some random delay
    matrix::fault::RandomPause(100_jfs, 800_jfs);

    auto& victim = matrix::fault::RandomServer(pool);
    auto& victim_2 = matrix::fault::RandomServer(pool);

    switch (node::rt::RandomNumber(5)) {
      case 0:
      case 1:
      case 2:
        // Reboot
        victim.FastReboot();
        if (victim.Name() != victim_2.Name()) {
          victim_2.FastReboot();
        }
        break;
      case 3:
        // Freeze
        victim.Pause();
        matrix::fault::RandomPause(100_jfs, 500_jfs);
        victim.Resume();
        break;
      case 4:
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
  auto pool = node::rt::Discovery()->ListPool("paxos");

  // Bound on number of crashes
  size_t bound = (pool.size() - 1) / 2;

  // [0, bound]
  size_t crashes = node::rt::RandomNumber(0, bound);

  LOG_INFO("Crash budget: {}", crashes);

  for (size_t i = 0; i < crashes; ++i) {
    matrix::fault::RandomPause(100_jfs, 1000_jfs);

    auto& victim = matrix::fault::RandomServer(pool);

    if (victim.IsAlive()) {
      victim.Crash();
    }
  }
}

//////////////////////////////////////////////////////////////////////

// Seed -> simulation digest
// Deterministic
size_t RunSimulation(size_t seed) {
  auto& runner = matrix::TestRunner::Access();

  static const Jiffies kTimeLimit = 100000_jfs;
  static const size_t kRequestsThreshold = 4;

  runner.Verbose() << "Simulation seed: " << seed << std::endl;

  matrix::Random random{seed};

  // Randomize simulation parameters
  const size_t replicas = random.Get(3, 5);
  const size_t clients = random.Get(2, 3);

  runner.Verbose() << "Parameters: "
                   << "replicas = " << replicas << ", "
                   << "clients = " << clients << std::endl;

  // Reset RPC ids
  commute::rpc::ResetIds();

  matrix::facade::World world{seed};

  runner.Configure(world);

  world.SetTimeModel(paxos::MakeTimeModel());

  // Cluster
  world.MakePool("paxos", paxos::NodeMain).Size(replicas);

  // Clients
  world.AddClients(Client, /*count=*/clients);

  // Adversaries

  if (random.Maybe(3)) {
    if (random.Maybe(7)) {
      // Network partitions
      runner.Verbose() << "Partitions" << std::endl;
      world.AddAdversary(NetAdversary);
    }

    if (random.Maybe(3)) {
      // Reboots, pauses
      runner.Verbose() << "Reboots" << std::endl;
      world.AddAdversary(NodeAdversary);
    }

    if (random.Maybe(7)) {
      // Crashes
      runner.Verbose() << "Crashes" << std::endl;
      world.AddAdversary(NodeReaper);
    }
  }

  // Globals
  world.InitCounter("requests", 0);

  world.SetGlobal<int64_t>("config.paxos.backoff.init", 100);
  world.SetGlobal<int64_t>("config.paxos.backoff.max", 2000);
  world.SetGlobal<int64_t>("config.paxos.backoff.factor", 2);

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

    runner.Report() << "Simulation for seed = " << seed << " failed: ";

    if (world.TimeElapsed() < kTimeLimit) {
      runner.Report() << "deadlock in simulation" << std::endl;
    } else {
      runner.Report() << "time limit exceeded" << std::endl;
    }
    runner.Fail();
  }

  // Check safety properties
  const auto history = world.History();
  const bool safe = consensus::IsSafe(history);

  if (!safe) {
    // Log
    runner.Verbose() << "Log:" << std::endl;
    matrix::WriteTextLog(event_log, runner.Verbose());
    runner.Verbose() << std::endl;

    // History
    runner.Report() << "History is NOT SAFE for seed = " << seed << ":"
                    << std::endl;
    semantics::Print<consensus::Printer>(history, runner.Report());

    runner.Fail();
  }

  return digest;
}

int main(int argc, const char** argv) {
  return matrix::Main(argc, argv, RunSimulation);
}
