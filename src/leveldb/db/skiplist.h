#ifndef STORAGE_LEVELDB_DB_SKIPLIST_H_
#define STORAGE_LEVELDB_DB_SKIPLIST_H_

// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Thread safety
// -------------
//
// Writes require external synchronization, most likely a mutex.
// Reads require a guarantee that the SkipList will not be destroyed
// while the read is in progress.  Apart from that, reads progress
// without any internal locking or synchronization.
//
// Invariants:
//
// (1) Allocated nodes are never deleted until the SkipList is
// destroyed.  This is trivially guaranteed by the code since we
// never delete any skip list nodes.
//
// (2) The contents of a Node except for the next/prev pointers are
// immutable after the Node has been linked into the SkipList.
// Only Insert() modifies the list, and it is careful to initialize

#include <assert.h>
#include <stdlib.h>
#include "port/port.h"
#include "util/arena.h"
#include "util/random.h"

namespace leveldb {

class Arena;

template <typename Key, class Comparator>
class SkipList {
private:
    struct Node;

public:
    explicit SkipList(Comparator cmp, Arena* arena);

    void Insert(const Key& key);

    bool Contains(const Key& key) const;

    class Iterator {
    public:
        explicit Iterator(const SkipList* list);

        bool Valid() const;

        const Key& key() const;

        void Next();

        void Prev();

        void Seek(const Key& target);

        void SeekToFirst();

        void SeekToLast();

    private:
        const SkipList* list_;
        Node* node_;
    };

private:
    enum { kMaxHeight = 12 };

    Comparator const compare_;
    Arena* const arena_; // Arena used for allocations of nodes

    Node* const head_;

    port::AtomicPointer max_height_; // Height of the entire list

    inline int GetMaxHeight() const
    {
        return static_cast<int>(
            reinterpret_cast<intptr_t>(max_height_.NoBarrier_Load()));
    }

    Random rnd_;

    Node* NewNode(const Key& key, int height);
    int RandomHeight();
    bool Equal(const Key& a, const Key& b) const { return (compare_(a, b) == 0); }

    bool KeyIsAfterNode(const Key& key, Node* n) const;

    Node* FindGreaterOrEqual(const Key& key, Node** prev) const;

    Node* FindLessThan(const Key& key) const;

    Node* FindLast() const;

    SkipList(const SkipList&);
    void operator=(const SkipList&);
};

template <typename Key, class Comparator>
struct SkipList<Key, Comparator>::Node {
    explicit Node(const Key& k)
        : key(k)
    {
    }

    Key const key;

    Node* Next(int n)
    {
        assert(n >= 0);

        return reinterpret_cast<Node*>(next_[n].Acquire_Load());
    }
    void SetNext(int n, Node* x)
    {
        assert(n >= 0);

        next_[n].Release_Store(x);
    }

    Node* NoBarrier_Next(int n)
    {
        assert(n >= 0);
        return reinterpret_cast<Node*>(next_[n].NoBarrier_Load());
    }
    void NoBarrier_SetNext(int n, Node* x)
    {
        assert(n >= 0);
        next_[n].NoBarrier_Store(x);
    }

private:
    port::AtomicPointer next_[1];
};

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::NewNode(const Key& key, int height)
{
    char* mem = arena_->AllocateAligned(
        sizeof(Node) + sizeof(port::AtomicPointer) * (height - 1));
    return new (mem) Node(key);
}

template <typename Key, class Comparator>
inline SkipList<Key, Comparator>::Iterator::Iterator(const SkipList* list)
{
    list_ = list;
    node_ = NULL;
}

template <typename Key, class Comparator>
inline bool SkipList<Key, Comparator>::Iterator::Valid() const
{
    return node_ != NULL;
}

template <typename Key, class Comparator>
inline const Key& SkipList<Key, Comparator>::Iterator::key() const
{
    assert(Valid());
    return node_->key;
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Next()
{
    assert(Valid());
    node_ = node_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Prev()
{

    assert(Valid());
    node_ = list_->FindLessThan(node_->key);
    if (node_ == list_->head_) {
        node_ = NULL;
    }
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::Seek(const Key& target)
{
    node_ = list_->FindGreaterOrEqual(target, NULL);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToFirst()
{
    node_ = list_->head_->Next(0);
}

template <typename Key, class Comparator>
inline void SkipList<Key, Comparator>::Iterator::SeekToLast()
{
    node_ = list_->FindLast();
    if (node_ == list_->head_) {
        node_ = NULL;
    }
}

template <typename Key, class Comparator>
int SkipList<Key, Comparator>::RandomHeight()
{

    static const unsigned int kBranching = 4;
    int height = 1;
    while (height < kMaxHeight && ((rnd_.Next() % kBranching) == 0)) {
        height++;
    }
    assert(height > 0);
    assert(height <= kMaxHeight);
    return height;
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::KeyIsAfterNode(const Key& key, Node* n) const
{

    return (n != NULL) && (compare_(n->key, key) < 0);
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindGreaterOrEqual(const Key& key, Node** prev)
    const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true) {
        Node* next = x->Next(level);
        if (KeyIsAfterNode(key, next)) {

            x = next;
        } else {
            if (prev != NULL)
                prev[level] = x;
            if (level == 0) {
                return next;
            } else {

                level--;
            }
        }
    }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node*
SkipList<Key, Comparator>::FindLessThan(const Key& key) const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true) {
        assert(x == head_ || compare_(x->key, key) < 0);
        Node* next = x->Next(level);
        if (next == NULL || compare_(next->key, key) >= 0) {
            if (level == 0) {
                return x;
            } else {

                level--;
            }
        } else {
            x = next;
        }
    }
}

template <typename Key, class Comparator>
typename SkipList<Key, Comparator>::Node* SkipList<Key, Comparator>::FindLast()
    const
{
    Node* x = head_;
    int level = GetMaxHeight() - 1;
    while (true) {
        Node* next = x->Next(level);
        if (next == NULL) {
            if (level == 0) {
                return x;
            } else {

                level--;
            }
        } else {
            x = next;
        }
    }
}

template <typename Key, class Comparator>
SkipList<Key, Comparator>::SkipList(Comparator cmp, Arena* arena)
    : compare_(cmp)
    , arena_(arena)
    , head_(NewNode(0 /* any key will do */, kMaxHeight))
    , max_height_(reinterpret_cast<void*>(1))
    , rnd_(0xdeadbeef)
{
    for (int i = 0; i < kMaxHeight; i++) {
        head_->SetNext(i, NULL);
    }
}

template <typename Key, class Comparator>
void SkipList<Key, Comparator>::Insert(const Key& key)
{

    Node* prev[kMaxHeight];
    Node* x = FindGreaterOrEqual(key, prev);

    assert(x == NULL || !Equal(key, x->key));

    int height = RandomHeight();
    if (height > GetMaxHeight()) {
        for (int i = GetMaxHeight(); i < height; i++) {
            prev[i] = head_;
        }

        max_height_.NoBarrier_Store(reinterpret_cast<void*>(height));
    }

    x = NewNode(key, height);
    for (int i = 0; i < height; i++) {

        x->NoBarrier_SetNext(i, prev[i]->NoBarrier_Next(i));
        prev[i]->SetNext(i, x);
    }
}

template <typename Key, class Comparator>
bool SkipList<Key, Comparator>::Contains(const Key& key) const
{
    Node* x = FindGreaterOrEqual(key, NULL);
    if (x != NULL && Equal(key, x->key)) {
        return true;
    } else {
        return false;
    }
}

} // namespace leveldb

#endif // STORAGE_LEVELDB_DB_SKIPLIST_H_
