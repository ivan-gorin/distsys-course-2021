#pragma once

#include <rsm/replica/store/log_entry.hpp>

#include <persist/fs/path.hpp>
#include <persist/rsm/multipaxos/log/log.hpp>

#include <memory>
#include <optional>

namespace rsm {

// Persistent log
// Indexed from 1
// NOT thread safe, external synchronization required

class Log {
 public:
  explicit Log(const persist::fs::Path& store_dir);

  // One-shot
  void Open();

  bool IsEmpty(size_t index) const;
  std::optional<LogEntry> Read(size_t index) const;

  void Update(size_t index, const LogEntry& entry);

  void TruncatePrefix(size_t index);

 private:
  using ILogImpl = persist::rsm::multipaxos::IRandomAccessLog;

  std::shared_ptr<ILogImpl> MakeLogImpl(const persist::fs::Path& store_dir);

 private:
  std::shared_ptr<ILogImpl> impl_;
};

}  // namespace rsm
