#pragma once

#include <rsm/proxy/client.hpp>

#include <commute/rpc/service_base.hpp>

namespace rsm {

class ProxyService : public commute::rpc::ServiceBase<ProxyService> {
 public:
  explicit ProxyService(const std::string& rsm_pool_name)
      : client_(rsm_pool_name) {
  }

  muesli::Bytes Execute(Command command) {
    // Forward request to RSM replica
    return client_.Execute(std::move(command));
  }

 protected:
  void RegisterMethods() override {
    COMMUTE_RPC_REGISTER_METHOD(Execute);
  }

 private:
  ProxyClient client_;
};

}  // namespace rsm
