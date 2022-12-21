#pragma once

#include <matrix/fault/listener.hpp>
#include <matrix/fault/access.hpp>

#include <commute/rpc/wire.hpp>

#include <optional>

class LeaderTracker {
 public:
  LeaderTracker(uint16_t rpc_port)
      : rpc_port_(rpc_port),
        listener_(whirl::matrix::fault::NetworkListener()) {
  }

  void Reset() {
    leader_.reset();
  }

  std::optional<std::string> Track() {
    while (listener_.FrameCount() > frame_index_) {
      const auto& frame = listener_.GetFrame(frame_index_++);
      Process(frame);
    }
    return leader_;
  }

 private:
  void Process(const whirl::matrix::net::Frame& frame) {
    if (frame.packet.header.type != whirl::matrix::net::Packet::Type::Data) {
      return;
    }
    if (frame.packet.header.dest_port != rpc_port_) {
      return;
    }
    // RPC request!
    auto rpc_request =
        muesli::Deserialize<commute::rpc::proto::Request>(frame.packet.message);
    if (rpc_request.method.name == "AppendEntries") {
      leader_.emplace(frame.header.source_host);
    }
  }

 private:
  const uint16_t rpc_port_;
  whirl::matrix::fault::INetworkListener& listener_;
  size_t frame_index_ = 0;
  std::optional<std::string> leader_;
};