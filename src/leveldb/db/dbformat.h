// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.

#ifndef STORAGE_LEVELDB_DB_DBFORMAT_H_
#define STORAGE_LEVELDB_DB_DBFORMAT_H_

#include <stdio.h>
#include "leveldb/comparator.h"
#include "leveldb/db.h"
#include "leveldb/filter_policy.h"
#include "leveldb/slice.h"
#include "leveldb/table_builder.h"
#include "util/coding.h"
#include "util/logging.h"

namespace leveldb {

// Grouping of constants.  We may want to make some of these
// parameters set via options.
namespace config {
    static const int kNumLevels = 7;

    // Level-0 compaction is started when we hit this many files.
    static const int kL0_CompactionTrigger = 4;

    static const int kL0_SlowdownWritesTrigger = 8;

    static const int kL0_StopWritesTrigger = 12;

    static const int kMaxMemCompactLevel = 2;

    static const int kReadBytesPeriod = 1048576;

} // namespace config

class InternalKey;

enum ValueType {
    kTypeDeletion = 0x0,
    kTypeValue = 0x1
};

static const ValueType kValueTypeForSeek = kTypeValue;

typedef uint64_t SequenceNumber;

static const SequenceNumber kMaxSequenceNumber = ((0x1ull << 56) - 1);

struct ParsedInternalKey {
    Slice user_key;
    SequenceNumber sequence;
    ValueType type;

    ParsedInternalKey() {} // Intentionally left uninitialized (for speed)
    ParsedInternalKey(const Slice& u, const SequenceNumber& seq, ValueType t)
        : user_key(u)
        , sequence(seq)
        , type(t)
    {
    }
    std::string DebugString() const;
};

inline size_t InternalKeyEncodingLength(const ParsedInternalKey& key)
{
    return key.user_key.size() + 8;
}

extern void AppendInternalKey(std::string* result,
                              const ParsedInternalKey& key);

extern bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result);

inline Slice ExtractUserKey(const Slice& internal_key)
{
    assert(internal_key.size() >= 8);
    return Slice(internal_key.data(), internal_key.size() - 8);
}

inline ValueType ExtractValueType(const Slice& internal_key)
{
    assert(internal_key.size() >= 8);
    const size_t n = internal_key.size();
    uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
    unsigned char c = num & 0xff;
    return static_cast<ValueType>(c);
}

class InternalKeyComparator : public Comparator {
private:
    const Comparator* user_comparator_;

public:
    explicit InternalKeyComparator(const Comparator* c)
        : user_comparator_(c)
    {
    }
    virtual const char* Name() const;
    virtual int Compare(const Slice& a, const Slice& b) const;
    virtual void FindShortestSeparator(
        std::string* start,
        const Slice& limit) const;
    virtual void FindShortSuccessor(std::string* key) const;

    const Comparator* user_comparator() const { return user_comparator_; }

    int Compare(const InternalKey& a, const InternalKey& b) const;
};

class InternalFilterPolicy : public FilterPolicy {
private:
    const FilterPolicy* const user_policy_;

public:
    explicit InternalFilterPolicy(const FilterPolicy* p)
        : user_policy_(p)
    {
    }
    virtual const char* Name() const;
    virtual void CreateFilter(const Slice* keys, int n, std::string* dst) const;
    virtual bool KeyMayMatch(const Slice& key, const Slice& filter) const;
};

class InternalKey {
private:
    std::string rep_;

public:
    InternalKey() {} // Leave rep_ as empty to indicate it is invalid
    InternalKey(const Slice& user_key, SequenceNumber s, ValueType t)
    {
        AppendInternalKey(&rep_, ParsedInternalKey(user_key, s, t));
    }

    void DecodeFrom(const Slice& s) { rep_.assign(s.data(), s.size()); }
    Slice Encode() const
    {
        assert(!rep_.empty());
        return rep_;
    }

    Slice user_key() const { return ExtractUserKey(rep_); }

    void SetFrom(const ParsedInternalKey& p)
    {
        rep_.clear();
        AppendInternalKey(&rep_, p);
    }

    void Clear() { rep_.clear(); }

    std::string DebugString() const;
};

inline int InternalKeyComparator::Compare(
    const InternalKey& a, const InternalKey& b) const
{
    return Compare(a.Encode(), b.Encode());
}

inline bool ParseInternalKey(const Slice& internal_key,
                             ParsedInternalKey* result)
{
    const size_t n = internal_key.size();
    if (n < 8)
        return false;
    uint64_t num = DecodeFixed64(internal_key.data() + n - 8);
    unsigned char c = num & 0xff;
    result->sequence = num >> 8;
    result->type = static_cast<ValueType>(c);
    result->user_key = Slice(internal_key.data(), n - 8);
    return (c <= static_cast<unsigned char>(kTypeValue));
}

class LookupKey {
public:
    LookupKey(const Slice& user_key, SequenceNumber sequence);

    ~LookupKey();

    Slice memtable_key() const { return Slice(start_, end_ - start_); }

    Slice internal_key() const { return Slice(kstart_, end_ - kstart_); }

    Slice user_key() const { return Slice(kstart_, end_ - kstart_ - 8); }

private:
    const char* start_;
    const char* kstart_;
    const char* end_;
    char space_[200]; // Avoid allocation for short keys

    LookupKey(const LookupKey&);
    void operator=(const LookupKey&);
};

inline LookupKey::~LookupKey()
{
    if (start_ != space_)
        delete[] start_;
}

} // namespace leveldb

#endif // STORAGE_LEVELDB_DB_DBFORMAT_H_
