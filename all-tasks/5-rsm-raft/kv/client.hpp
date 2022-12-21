#pragma once

#include <rsm/client/client.hpp>
#include <kv/operations.hpp>

namespace kv {

class Client {
 public:
  explicit Client(commute::rpc::IChannelPtr channel) : rsm_client_(channel) {
  }

  void Set(Key key, Value value) {
    Execute<kv::Set>({key, value});
  }

  Value Get(Key key) {
    auto response = Execute<kv::Get>({key});
    return response.value;
  }

  Value Cas(Key key, Value expected, Value desired) {
    auto response = Execute<kv::Cas>({key, expected, desired});
    return response.old_value;
  }

 private:
  template <typename Op>
  typename Op::Response Execute(typename Op::Request request) {
    auto response = rsm_client_.Execute(Op::Type(), muesli::Serialize(request),
                                        Op::ReadOnly());

    return muesli::Deserialize<typename Op::Response>(response);
  }

 private:
  rsm::Client rsm_client_;
};

}  // namespace kv
