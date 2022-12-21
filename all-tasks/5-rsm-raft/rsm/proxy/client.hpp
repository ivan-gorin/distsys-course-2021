#pragma once

#include <timber/log.hpp>

#include <rsm/client/command.hpp>

#include <muesli/serialize.hpp>

#include <commute/rpc/channel.hpp>

#include <await/fibers/sync/mutex.hpp>

#include <map>
#include <optional>
#include <vector>

namespace rsm {

class ProxyClient {
 public:
  explicit ProxyClient(const std::string& rsm_pool_name);

  muesli::Bytes Execute(const Command& cmd);

 private:
  void ConnectToRSM(const std::string& pool_name);
  commute::rpc::IChannelPtr Channel(const std::string& host);

  static bool IsRetriable(const std::error_code& error);

  void CacheLeader(const std::string& host);
  void ForgetLeader();
  std::string PickRandomReplica();

  std::string ChooseReplica(const Command& cmd);

 private:
  std::vector<std::string> replicas_;
  std::map<std::string, commute::rpc::IChannelPtr> channels_;

  await::fibers::Mutex mutex_;  // Guards leader_
  std::optional<std::string> leader_;

  timber::Logger logger_;
};

}  // namespace rsm
