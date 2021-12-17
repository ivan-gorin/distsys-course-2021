#pragma once

#include <rsm/replica/store/log_entry.hpp>

#include <persist/fs/path.hpp>
#include <persist/rsm/raft/log/log.hpp>

namespace rsm {

// Persistent log
// Indexed from 1
// NOT thread safe, external synchronization required

class Log {
 public:
  Log(persist::fs::IFileSystem* fs, const persist::fs::Path& store_dir);

  // One-shot
  void Open();

  LogEntry Read(size_t index) const;

  size_t Length() const;

  // Append entries[start_offset:]
  void Append(const LogEntries& entries, size_t start_offset = 0);

  void TruncateSuffix(size_t from_index);

  uint64_t Term(size_t index) const;

  // For leader election
  uint64_t LastLogTerm() const;

  // Compaction
  void TruncatePrefix(size_t index);

 private:
  using ILogImpl = persist::rsm::raft::IContinuousLog;

  std::shared_ptr<ILogImpl> MakeLogImpl(persist::fs::IFileSystem* fs,
                                        const persist::fs::Path& store_dir);

 private:
  std::shared_ptr<ILogImpl> impl_;
};

}  // namespace rsm
