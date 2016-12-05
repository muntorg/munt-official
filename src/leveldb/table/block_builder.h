// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
#define STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_

#include <vector>

#include <stdint.h>
#include "leveldb/slice.h"

namespace leveldb {

struct Options;

class BlockBuilder {
public:
    explicit BlockBuilder(const Options* options);

    // Reset the contents as if the BlockBuilder was just constructed.
    void Reset();

    // REQUIRES: Finish() has not been called since the last call to Reset().

    void Add(const Slice& key, const Slice& value);

    Slice Finish();

    size_t CurrentSizeEstimate() const;

    bool empty() const
    {
        return buffer_.empty();
    }

private:
    const Options* options_;
    std::string buffer_; // Destination buffer
    std::vector<uint32_t> restarts_; // Restart points
    int counter_; // Number of entries emitted since restart
    bool finished_; // Has Finish() been called?
    std::string last_key_;

    BlockBuilder(const BlockBuilder&);
    void operator=(const BlockBuilder&);
};

} // namespace leveldb

#endif // STORAGE_LEVELDB_TABLE_BLOCK_BUILDER_H_
