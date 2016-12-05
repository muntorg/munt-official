// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_FORMAT_H_
#define STORAGE_LEVELDB_TABLE_FORMAT_H_

#include <string>
#include <stdint.h>
#include "leveldb/slice.h"
#include "leveldb/status.h"
#include "leveldb/table_builder.h"

namespace leveldb {

class Block;
class RandomAccessFile;
struct ReadOptions;

// BlockHandle is a pointer to the extent of a file that stores a data
// block or a meta block.
class BlockHandle {
public:
    BlockHandle();

    uint64_t offset() const { return offset_; }
    void set_offset(uint64_t offset) { offset_ = offset; }

    uint64_t size() const { return size_; }
    void set_size(uint64_t size) { size_ = size; }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

    enum { kMaxEncodedLength = 10 + 10 };

private:
    uint64_t offset_;
    uint64_t size_;
};

class Footer {
public:
    Footer() {}

    const BlockHandle& metaindex_handle() const { return metaindex_handle_; }
    void set_metaindex_handle(const BlockHandle& h) { metaindex_handle_ = h; }

    const BlockHandle& index_handle() const
    {
        return index_handle_;
    }
    void set_index_handle(const BlockHandle& h)
    {
        index_handle_ = h;
    }

    void EncodeTo(std::string* dst) const;
    Status DecodeFrom(Slice* input);

    enum {
        kEncodedLength = 2 * BlockHandle::kMaxEncodedLength + 8
    };

private:
    BlockHandle metaindex_handle_;
    BlockHandle index_handle_;
};

static const uint64_t kTableMagicNumber = 0xdb4775248b80fb57ull;

static const size_t kBlockTrailerSize = 5;

struct BlockContents {
    Slice data; // Actual contents of data
    bool cachable; // True iff data can be cached
    bool heap_allocated; // True iff caller should delete[] data.data()
};

extern Status ReadBlock(RandomAccessFile* file,
                        const ReadOptions& options,
                        const BlockHandle& handle,
                        BlockContents* result);

inline BlockHandle::BlockHandle()
    : offset_(~static_cast<uint64_t>(0))
    , size_(~static_cast<uint64_t>(0))
{
}

} // namespace leveldb

#endif // STORAGE_LEVELDB_TABLE_FORMAT_H_
