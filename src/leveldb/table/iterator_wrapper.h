// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_TABLE_ITERATOR_WRAPPER_H_
#define STORAGE_LEVELDB_TABLE_ITERATOR_WRAPPER_H_

namespace leveldb {

// A internal wrapper class with an interface similar to Iterator that
// caches the valid() and key() results for an underlying iterator.
// This can help avoid virtual function calls and also gives better
// cache locality.
class IteratorWrapper {
public:
    IteratorWrapper()
        : iter_(NULL)
        , valid_(false)
    {
    }
    explicit IteratorWrapper(Iterator* iter)
        : iter_(NULL)
    {
        Set(iter);
    }
    ~IteratorWrapper() { delete iter_; }
    Iterator* iter() const { return iter_; }

    void Set(Iterator* iter)
    {
        delete iter_;
        iter_ = iter;
        if (iter_ == NULL) {
            valid_ = false;
        } else {
            Update();
        }
    }

    bool Valid() const { return valid_; }
    Slice key() const
    {
        assert(Valid());
        return key_;
    }
    Slice value() const
    {
        assert(Valid());
        return iter_->value();
    }

    Status status() const
    {
        assert(iter_);
        return iter_->status();
    }
    void Next()
    {
        assert(iter_);
        iter_->Next();
        Update();
    }
    void Prev()
    {
        assert(iter_);
        iter_->Prev();
        Update();
    }
    void Seek(const Slice& k)
    {
        assert(iter_);
        iter_->Seek(k);
        Update();
    }
    void SeekToFirst()
    {
        assert(iter_);
        iter_->SeekToFirst();
        Update();
    }
    void SeekToLast()
    {
        assert(iter_);
        iter_->SeekToLast();
        Update();
    }

private:
    void Update()
    {
        valid_ = iter_->Valid();
        if (valid_) {
            key_ = iter_->key();
        }
    }

    Iterator* iter_;
    bool valid_;
    Slice key_;
};

} // namespace leveldb

#endif // STORAGE_LEVELDB_TABLE_ITERATOR_WRAPPER_H_
