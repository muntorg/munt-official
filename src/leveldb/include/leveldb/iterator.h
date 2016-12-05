// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// An iterator yields a sequence of key/value pairs from a source.
// The following class defines the interface.  Multiple implementations
// are provided by this library.  In particular, iterators are provided
// to access the contents of a Table or a DB.
//
// Multiple threads can invoke const methods on an Iterator without
// external synchronization, but if any of the threads may call a
// non-const method, all threads accessing the same Iterator must use
// external synchronization.

#ifndef STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
#define STORAGE_LEVELDB_INCLUDE_ITERATOR_H_

#include "leveldb/slice.h"
#include "leveldb/status.h"

namespace leveldb {

class Iterator {
public:
    Iterator();
    virtual ~Iterator();

    virtual bool Valid() const = 0;

    virtual void SeekToFirst() = 0;

    virtual void SeekToLast() = 0;

    virtual void Seek(const Slice& target) = 0;

    virtual void Next() = 0;

    virtual void Prev() = 0;

    virtual Slice key() const = 0;

    virtual Slice value() const = 0;

    virtual Status status() const = 0;

    typedef void (*CleanupFunction)(void* arg1, void* arg2);
    void RegisterCleanup(CleanupFunction function, void* arg1, void* arg2);

private:
    struct Cleanup {
        CleanupFunction function;
        void* arg1;
        void* arg2;
        Cleanup* next;
    };
    Cleanup cleanup_;

    Iterator(const Iterator&);
    void operator=(const Iterator&);
};

extern Iterator* NewEmptyIterator();

extern Iterator* NewErrorIterator(const Status& status);

} // namespace leveldb

#endif // STORAGE_LEVELDB_INCLUDE_ITERATOR_H_
