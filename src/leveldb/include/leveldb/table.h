// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_TABLE_H_
#define STORAGE_LEVELDB_INCLUDE_TABLE_H_

#include <stdint.h>
#include "leveldb/iterator.h"

namespace leveldb {

class Block;
class BlockHandle;
class Footer;
struct Options;
class RandomAccessFile;
struct ReadOptions;
class TableCache;

// A Table is a sorted map from strings to strings.  Tables are
// immutable and persistent.  A Table may be safely accessed from
// multiple threads without external synchronization.
class Table {
public:
    static Status Open(const Options& options,
                       RandomAccessFile* file,
                       uint64_t file_size,
                       Table** table);

    ~Table();

    Iterator* NewIterator(const ReadOptions&) const;

    uint64_t ApproximateOffsetOf(const Slice& key) const;

private:
    struct Rep;
    Rep* rep_;

    explicit Table(Rep* rep) { rep_ = rep; }
    static Iterator* BlockReader(void*, const ReadOptions&, const Slice&);

    friend class TableCache;
    Status InternalGet(
        const ReadOptions&, const Slice& key,
        void* arg,
        void (*handle_result)(void* arg, const Slice& k, const Slice& v));

    void ReadMeta(const Footer& footer);
    void ReadFilter(const Slice& filter_handle_value);

    Table(const Table&);
    void operator=(const Table&);
};

} // namespace leveldb

#endif // STORAGE_LEVELDB_INCLUDE_TABLE_H_
