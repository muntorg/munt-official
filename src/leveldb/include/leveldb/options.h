// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
#define STORAGE_LEVELDB_INCLUDE_OPTIONS_H_

#include <stddef.h>

namespace leveldb {

class Cache;
class Comparator;
class Env;
class FilterPolicy;
class Logger;
class Snapshot;

// DB contents are stored in a set of blocks, each of which holds a
// sequence of key,value pairs.  Each block may be compressed before
// being stored in a file.  The following enum describes which
// compression method (if any) is used to compress a block.
enum CompressionType {
    // NOTE: do not change the values of existing entries, as these are

    kNoCompression = 0x0,
    kSnappyCompression = 0x1
};

struct Options {

    const Comparator* comparator;

    bool create_if_missing;

    bool error_if_exists;

    bool paranoid_checks;

    Env* env;

    Logger* info_log;

    size_t write_buffer_size;

    int max_open_files;

    Cache* block_cache;

    size_t block_size;

    int block_restart_interval;

    CompressionType compression;

    const FilterPolicy* filter_policy;

    Options();
};

struct ReadOptions {

    bool verify_checksums;

    bool fill_cache;

    const Snapshot* snapshot;

    ReadOptions()
        : verify_checksums(false)
        , fill_cache(true)
        , snapshot(NULL)
    {
    }
};

struct WriteOptions {

    bool sync;

    WriteOptions()
        : sync(false)
    {
    }
};

} // namespace leveldb

#endif // STORAGE_LEVELDB_INCLUDE_OPTIONS_H_
