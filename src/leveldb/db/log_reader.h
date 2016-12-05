// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_LOG_READER_H_
#define STORAGE_LEVELDB_DB_LOG_READER_H_

#include <stdint.h>

#include "db/log_format.h"
#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class SequentialFile;

namespace log {

    class Reader {
    public:
        // Interface for reporting errors.
        class Reporter {
        public:
            virtual ~Reporter();

            virtual void Corruption(size_t bytes, const Status& status) = 0;
        };

        Reader(SequentialFile* file, Reporter* reporter, bool checksum,
               uint64_t initial_offset);

        ~Reader();

        bool ReadRecord(Slice* record, std::string* scratch);

        uint64_t LastRecordOffset();

    private:
        SequentialFile* const file_;
        Reporter* const reporter_;
        bool const checksum_;
        char* const backing_store_;
        Slice buffer_;
        bool eof_; // Last Read() indicated EOF by returning < kBlockSize

        uint64_t last_record_offset_;

        uint64_t end_of_buffer_offset_;

        uint64_t const initial_offset_;

        enum {
            kEof = kMaxRecordType + 1,

            kBadRecord = kMaxRecordType + 2
        };

        bool SkipToInitialBlock();

        unsigned int ReadPhysicalRecord(Slice* result);

        void ReportCorruption(uint64_t bytes, const char* reason);
        void ReportDrop(uint64_t bytes, const Status& reason);

        Reader(const Reader&);
        void operator=(const Reader&);
    };

} // namespace log
} // namespace leveldb

#endif // STORAGE_LEVELDB_DB_LOG_READER_H_
