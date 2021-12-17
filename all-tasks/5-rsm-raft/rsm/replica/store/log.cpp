#include <rsm/replica/store/log.hpp>

#include <persist/rsm/raft/log/file.hpp>

#include <muesli/serialize.hpp>

namespace rsm {

Log::Log(persist::fs::IFileSystem* fs, const persist::fs::Path& store_dir)
    : impl_(MakeLogImpl(fs, store_dir)) {
}

void Log::Open() {
  impl_->Open();
}

LogEntry Log::Read(size_t index) const {
  auto bytes = impl_->Read(index);
  return muesli::Deserialize<LogEntry>(bytes);
}

size_t Log::Length() const {
  return impl_->Length();
}

uint64_t Log::Term(size_t index) const {
  return Read(index).term;
}

uint64_t Log::LastLogTerm() const {
  size_t length = Length();
  if (length == 0) {
    return 0;
  }
  return Read(length).term;
}

void Log::Append(const LogEntries& entries, size_t start_offset) {
  persist::rsm::raft::Entries persist_entries;
  for (size_t i = start_offset; i < entries.size(); ++i) {
    persist_entries.push_back(muesli::Serialize(entries[i]));
  }
  impl_->Append(persist_entries);
}

void Log::TruncateSuffix(size_t from_index) {
  impl_->TruncateSuffix(from_index);
}

void Log::TruncatePrefix(size_t end_index) {
  impl_->TruncatePrefix(end_index);
}

std::shared_ptr<Log::ILogImpl> Log::MakeLogImpl(
    persist::fs::IFileSystem* fs, const persist::fs::Path& store_dir) {
  const auto log_path = store_dir / "log";
  return std::make_shared<persist::rsm::raft::FileLog>(fs, log_path);
}

}  // namespace rsm
