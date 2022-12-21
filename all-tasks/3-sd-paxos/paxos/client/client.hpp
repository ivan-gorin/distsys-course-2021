#pragma once

#include <commute/rpc/channel.hpp>

#include <string>

namespace paxos {

class BlockingClient {
  using Value = std::string;

 public:
  explicit BlockingClient(commute::rpc::IChannelPtr channel)
    : channel_(std::move(channel)) {
  }

  Value Propose(Value value);

 private:
  static std::string GenerateTraceId(const Value& value);

 private:
  commute::rpc::IChannelPtr channel_;
};

}  // namespace paxos
