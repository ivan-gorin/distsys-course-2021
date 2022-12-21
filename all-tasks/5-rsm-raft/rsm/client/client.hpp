#pragma once

#include <timber/log.hpp>

#include <rsm/client/command.hpp>

#include <muesli/serialize.hpp>

#include <commute/rpc/channel.hpp>

#include <whirl/node/time/jiffies.hpp>

namespace rsm {

// Blocking client for RSM

class Client {
 public:
  explicit Client(commute::rpc::IChannelPtr proxies);

  muesli::Bytes Execute(std::string type, muesli::Bytes request, bool readonly);

 private:
  void GenerateClientId();
  RequestId NextRequestId();

  commute::rpc::TraceId MakeTraceId(const Command& cmd);

 private:
  commute::rpc::IChannelPtr proxies_;

  std::string client_id_;
  uint64_t request_index_{0};
};

}  // namespace rsm
