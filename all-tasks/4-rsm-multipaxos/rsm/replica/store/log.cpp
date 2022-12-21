#include <rsm/replica/store/log.hpp>

#include <persist/rsm/multipaxos/log/file.hpp>
#include <persist/rsm/multipaxos/log/segmented/log.hpp>

#include <muesli/serialize.hpp>

#include <whirl/node/runtime/shortcuts.hpp>

namespace rsm {

Log::Log(const persist::fs::Path& store_dir) : impl_(MakeLogImpl(store_dir)) {
}

void Log::Open() {
  impl_->Open();
}

bool Log::IsEmpty(size_t index) const {
  return impl_->IsEmpty(index);
}

std::optional<LogEntry> Log::Read(size_t index) const {
  auto entry = impl_->TryRead(index);
  if (entry.has_value()) {
    return muesli::Deserialize<LogEntry>(*entry);
  }
  return std::nullopt;
}

void Log::Update(size_t index, const LogEntry& entry) {
  impl_->Update(index, muesli::Serialize(entry));
}

void Log::TruncatePrefix(size_t end_index) {
  impl_->TruncatePrefix(end_index);
}

std::shared_ptr<Log::ILogImpl> Log::MakeLogImpl(
    const persist::fs::Path& store_dir) {
  const auto log_path = store_dir / "log";

  return std::make_shared<persist::rsm::multipaxos::FileLog>(
      whirl::node::rt::Fs(), log_path);
}

}  // namespace rsm
