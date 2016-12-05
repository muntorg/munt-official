// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DB_IMPL_H_
#define STORAGE_LEVELDB_DB_DB_IMPL_H_

#include <deque>
#include <set>
#include "db/dbformat.h"
#include "db/log_writer.h"
#include "db/snapshot.h"
#include "leveldb/db.h"
#include "leveldb/env.h"
#include "port/port.h"
#include "port/thread_annotations.h"

namespace leveldb {

class MemTable;
class TableCache;
class Version;
class VersionEdit;
class VersionSet;

class DBImpl : public DB {
public:
    DBImpl(const Options& options, const std::string& dbname);
    virtual ~DBImpl();

    virtual Status Put(const WriteOptions&, const Slice& key, const Slice& value);
    virtual Status Delete(const WriteOptions&, const Slice& key);
    virtual Status Write(const WriteOptions& options, WriteBatch* updates);
    virtual Status Get(const ReadOptions& options,
                       const Slice& key,
                       std::string* value);
    virtual Iterator* NewIterator(const ReadOptions&);
    virtual const Snapshot* GetSnapshot();
    virtual void ReleaseSnapshot(const Snapshot* snapshot);
    virtual bool GetProperty(const Slice& property, std::string* value);
    virtual void GetApproximateSizes(const Range* range, int n, uint64_t* sizes);
    virtual void CompactRange(const Slice* begin, const Slice* end);

    void TEST_CompactRange(int level, const Slice* begin, const Slice* end);

    Status TEST_CompactMemTable();

    Iterator* TEST_NewInternalIterator();

    int64_t TEST_MaxNextLevelOverlappingBytes();

    void RecordReadSample(Slice key);

private:
    friend class DB;
    struct CompactionState;
    struct Writer;

    Iterator* NewInternalIterator(const ReadOptions&,
                                  SequenceNumber* latest_snapshot,
                                  uint32_t* seed);

    Status NewDB();

    Status Recover(VersionEdit* edit) EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    void MaybeIgnoreError(Status* s) const;

    void DeleteObsoleteFiles();

    void CompactMemTable() EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    Status RecoverLogFile(uint64_t log_number,
                          VersionEdit* edit,
                          SequenceNumber* max_sequence)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    Status WriteLevel0Table(MemTable* mem, VersionEdit* edit, Version* base)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    Status MakeRoomForWrite(bool force /* compact even if there is room? */)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    WriteBatch* BuildBatchGroup(Writer** last_writer);

    void RecordBackgroundError(const Status& s);

    void MaybeScheduleCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    static void BGWork(void* db);
    void BackgroundCall();
    void BackgroundCompaction() EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    void CleanupCompaction(CompactionState* compact)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);
    Status DoCompactionWork(CompactionState* compact)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    Status OpenCompactionOutputFile(CompactionState* compact);
    Status FinishCompactionOutputFile(CompactionState* compact, Iterator* input);
    Status InstallCompactionResults(CompactionState* compact)
        EXCLUSIVE_LOCKS_REQUIRED(mutex_);

    Env* const env_;
    const InternalKeyComparator internal_comparator_;
    const InternalFilterPolicy internal_filter_policy_;
    const Options options_; // options_.comparator == &internal_comparator_
    bool owns_info_log_;
    bool owns_cache_;
    const std::string dbname_;

    TableCache* table_cache_;

    FileLock* db_lock_;

    port::Mutex mutex_;
    port::AtomicPointer shutting_down_;
    port::CondVar bg_cv_; // Signalled when background work finishes
    MemTable* mem_;
    MemTable* imm_; // Memtable being compacted
    port::AtomicPointer has_imm_; // So bg thread can detect non-NULL imm_
    WritableFile* logfile_;
    uint64_t logfile_number_;
    log::Writer* log_;
    uint32_t seed_; // For sampling.

    std::deque<Writer*> writers_;
    WriteBatch* tmp_batch_;

    SnapshotList snapshots_;

    std::set<uint64_t> pending_outputs_;

    bool bg_compaction_scheduled_;

    struct ManualCompaction {
        int level;
        bool done;
        const InternalKey* begin; // NULL means beginning of key range
        const InternalKey* end; // NULL means end of key range
        InternalKey tmp_storage; // Used to keep track of compaction progress
    };
    ManualCompaction* manual_compaction_;

    VersionSet* versions_;

    Status bg_error_;

    struct CompactionStats {
        int64_t micros;
        int64_t bytes_read;
        int64_t bytes_written;

        CompactionStats()
            : micros(0)
            , bytes_read(0)
            , bytes_written(0)
        {
        }

        void Add(const CompactionStats& c)
        {
            this->micros += c.micros;
            this->bytes_read += c.bytes_read;
            this->bytes_written += c.bytes_written;
        }
    };
    CompactionStats stats_[config::kNumLevels];

    DBImpl(const DBImpl&);
    void operator=(const DBImpl&);

    const Comparator* user_comparator() const
    {
        return internal_comparator_.user_comparator();
    }
};

extern Options SanitizeOptions(const std::string& db,
                               const InternalKeyComparator* icmp,
                               const InternalFilterPolicy* ipolicy,
                               const Options& src);

} // namespace leveldb

#endif // STORAGE_LEVELDB_DB_DB_IMPL_H_
