#pragma once

#include <rpc/method.hpp>
#include <rpc/message.hpp>

#include <map>
#include <memory>
#include <string>
#include <functional>

//////////////////////////////////////////////////////////////////////

struct ITestService {
  virtual ~ITestService() = default;

  virtual std::string_view Name() const = 0;
  virtual rpc::Message Call(rpc::Method method, rpc::Message request) = 0;
};

using ITestServicePtr = std::shared_ptr<ITestService>;

//////////////////////////////////////////////////////////////////////

class TestService : public ITestService {
  using Method = std::function<rpc::Message(rpc::Message)>;

 public:
  explicit TestService(std::string name) : name_(std::move(name)) {
  }

  void Add(const std::string& name, Method method) {
    methods_.emplace(name, std::move(method));
  }

  std::string_view Name() const override {
    return name_;
  }

  rpc::Message Call(std::string method_name, rpc::Message request) override {
    auto it = methods_.find(method_name);
    return it->second(std::move(request));
  }

 private:
  std::string name_;
  std::map<std::string, Method> methods_;
};
