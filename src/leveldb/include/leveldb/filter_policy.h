// Copyright (c) 2012 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// A database can be configured with a custom FilterPolicy object.
// This object is responsible for creating a small filter from a set
// of keys.  These filters are stored in leveldb and are consulted
// automatically by leveldb to decide whether or not to read some
// information from disk. In many cases, a filter can cut down the
// number of disk seeks form a handful to a single disk seek per
// DB::Get() call.
//
// Most people will want to use the builtin bloom filter support (see
// NewBloomFilterPolicy() below).

#ifndef STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
#define STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_

#include <string>

namespace leveldb {

class Slice;

class FilterPolicy {
public:
    virtual ~FilterPolicy();

    virtual const char* Name() const = 0;

    virtual void CreateFilter(const Slice* keys, int n, std::string* dst)
        const = 0;

    virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const = 0;
};

extern const FilterPolicy* NewBloomFilterPolicy(int bits_per_key);
}

#endif // STORAGE_LEVELDB_INCLUDE_FILTER_POLICY_H_
