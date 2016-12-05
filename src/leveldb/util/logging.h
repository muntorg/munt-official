// Copyright (c) 2011 The LevelDB Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file. See the AUTHORS file for names of contributors.
//
// Must not be included from any .h files to avoid polluting the namespace
// with macros.

#ifndef STORAGE_LEVELDB_UTIL_LOGGING_H_
#define STORAGE_LEVELDB_UTIL_LOGGING_H_

#include <stdio.h>
#include <stdint.h>
#include <string>
#include "port/port.h"

namespace leveldb {

class Slice;
class WritableFile;

// Append a human-readable printout of "num" to *str
extern void AppendNumberTo(std::string* str, uint64_t num);

// Append a human-readable printout of "value" to *str.

extern void AppendEscapedStringTo(std::string* str, const Slice& value);

extern std::string NumberToString(uint64_t num);

extern std::string EscapeString(const Slice& value);

extern bool ConsumeDecimalNumber(Slice* in, uint64_t* val);

} // namespace leveldb

#endif // STORAGE_LEVELDB_UTIL_LOGGING_H_
