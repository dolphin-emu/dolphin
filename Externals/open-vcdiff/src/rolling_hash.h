// Copyright 2007, 2008 The open-vcdiff Authors. All Rights Reserved.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#ifndef OPEN_VCDIFF_ROLLING_HASH_H_
#define OPEN_VCDIFF_ROLLING_HASH_H_

#include "config.h"
#include <stdint.h>  // uint32_t
#include "compile_assert.h"
#include "logging.h"

namespace open_vcdiff {

// Rabin-Karp hasher module -- this is a faster version with different
// constants, so it's not quite Rabin-Karp fingerprinting, but its behavior is
// close enough for most applications.

// Definitions common to all hash window sizes.
class RollingHashUtil {
 public:
  // Multiplier for incremental hashing.  The compiler should be smart enough to
  // convert (val * kMult) into ((val << 8) + val).
  static const uint32_t kMult = 257;

  // All hashes are returned modulo "kBase".  Current implementation requires
  // kBase <= 2^32/kMult to avoid overflow.  Also, kBase must be a power of two
  // so that we can compute modulus efficiently.
  static const uint32_t kBase = (1 << 23);

  // Returns operand % kBase, assuming that kBase is a power of two.
  static inline uint32_t ModBase(uint32_t operand) {
    return operand & (kBase - 1);
  }

  // Given an unsigned integer "operand", returns an unsigned integer "result"
  // such that
  //     result < kBase
  // and
  //     ModBase(operand + result) == 0
  static inline uint32_t FindModBaseInverse(uint32_t operand) {
    // The subtraction (0 - operand) produces an unsigned underflow for any
    // operand except 0.  The underflow results in a (very large) unsigned
    // number.  Binary subtraction is used instead of unary negation because
    // some compilers (e.g. Visual Studio 7+) produce a warning if an unsigned
    // value is negated.
    //
    // The C++ mod operation (operand % kBase) may produce different results for
    // different compilers if operand is negative.  That is not a problem in
    // this case, since all numbers used are unsigned, and ModBase does its work
    // using bitwise arithmetic rather than the % operator.
    return ModBase(uint32_t(0) - operand);
  }

  // Here's the heart of the hash algorithm.  Start with a partial_hash value of
  // 0, and run this HashStep once against each byte in the data window to be
  // hashed.  The result will be the hash value for the entire data window.  The
  // Hash() function, below, does exactly this, albeit with some refinements.
  static inline uint32_t HashStep(uint32_t partial_hash,
                                  unsigned char next_byte) {
    return ModBase((partial_hash * kMult) + next_byte);
  }

  // Use this function to start computing a new hash value based on the first
  // two bytes in the window.  It is equivalent to calling
  //     HashStep(HashStep(0, ptr[0]), ptr[1])
  // but takes advantage of the fact that the maximum value of
  // (ptr[0] * kMult) + ptr[1] is not large enough to exceed kBase, thus
  // avoiding an unnecessary ModBase operation.
  static inline uint32_t HashFirstTwoBytes(const char* ptr) {
    return (static_cast<unsigned char>(ptr[0]) * kMult)
        + static_cast<unsigned char>(ptr[1]);
  }
 private:
  // Making these private avoids copy constructor and assignment operator.
  // No objects of this type should be constructed.
  RollingHashUtil();
  RollingHashUtil(const RollingHashUtil&);  // NOLINT
  void operator=(const RollingHashUtil&);
};

// window_size must be >= 2.
template<int window_size>
class RollingHash {
 public:
  // Perform global initialization that is required in order to instantiate a
  // RollingHash.  This function *must* be called (preferably on startup) by any
  // program that uses a RollingHash.  It is harmless to call this function more
  // than once.  It is not thread-safe, but calling it from two different
  // threads at the same time can only cause a memory leak, not incorrect
  // behavior.  Make sure to call it before spawning any threads that could use
  // RollingHash.
  static void Init();

  // Initialize hasher to maintain a window of the specified size.  You need an
  // instance of this type to use UpdateHash(), but Hash() does not depend on
  // remove_table_, so it is static.
  RollingHash() {
    if (!remove_table_) {
      VCD_DFATAL << "RollingHash object instantiated"
                    " before calling RollingHash::Init()" << VCD_ENDL;
    }
  }

  // Compute a hash of the window "ptr[0, window_size - 1]".
  static uint32_t Hash(const char* ptr) {
    uint32_t h = RollingHashUtil::HashFirstTwoBytes(ptr);
    for (int i = 2; i < window_size; ++i) {
      h = RollingHashUtil::HashStep(h, ptr[i]);
    }
    return h;
  }

  // Update a hash by removing the oldest byte and adding a new byte.
  //
  // UpdateHash takes the hash value of buffer[0] ... buffer[window_size -1]
  // along with the value of buffer[0] (the "old_first_byte" argument)
  // and the value of buffer[window_size] (the "new_last_byte" argument).
  // It quickly computes the hash value of buffer[1] ... buffer[window_size]
  // without having to run Hash() on the entire window.
  //
  // The larger the window, the more advantage comes from using UpdateHash()
  // (which runs in time independent of window_size) instead of Hash().
  // Each time window_size doubles, the time to execute Hash() also doubles,
  // while the time to execute UpdateHash() remains constant.  Empirical tests
  // have borne out this statement.
  uint32_t UpdateHash(uint32_t old_hash,
                      const char old_first_byte,
                      const char new_last_byte) const {
    uint32_t partial_hash = RemoveFirstByteFromHash(old_hash, old_first_byte);
    return RollingHashUtil::HashStep(partial_hash, new_last_byte);
  }

 protected:
  // Given a full hash value for buffer[0] ... buffer[window_size -1], plus the
  // value of the first byte buffer[0], this function returns a *partial* hash
  // value for buffer[1] ... buffer[window_size -1].  See the comments in
  // Init(), below, for a description of how the contents of remove_table_ are
  // computed.
  static uint32_t RemoveFirstByteFromHash(uint32_t full_hash,
                                          unsigned char first_byte) {
    return RollingHashUtil::ModBase(full_hash + remove_table_[first_byte]);
  }

 private:
  // We keep a table that maps from any byte "b" to
  //    (- b * pow(kMult, window_size - 1)) % kBase
  static const uint32_t* remove_table_;
};

// For each window_size, fill a 256-entry table such that
//        the hash value of buffer[0] ... buffer[window_size - 1]
//      + remove_table_[buffer[0]]
//     == the hash value of buffer[1] ... buffer[window_size - 1]
// See the comments in Init(), below, for a description of how the contents of
// remove_table_ are computed.
template<int window_size>
const uint32_t* RollingHash<window_size>::remove_table_ = NULL;

// Init() checks to make sure that the static object remove_table_ has been
// initialized; if not, it does the considerable work of populating it.  Once
// it's ready, the table can be used for any number of RollingHash objects of
// the same window_size.
//
template<int window_size>
void RollingHash<window_size>::Init() {
  VCD_COMPILE_ASSERT(window_size >= 2,
                     RollingHash_window_size_must_be_at_least_2);
  if (remove_table_ == NULL) {
    // The new object is placed into a local pointer instead of directly into
    // remove_table_, for two reasons:
    //   1. remove_table_ is a pointer to const.  The table is populated using
    //      the non-const local pointer and then assigned to the global const
    //      pointer once it's ready.
    //   2. No other thread will ever see remove_table_ pointing to a
    //      partially-initialized table.  If two threads happen to call Init()
    //      at the same time, two tables with the same contents may be created
    //      (causing a memory leak), but the results will be consistent
    //      no matter which of the two tables is used.
    uint32_t* new_remove_table = new uint32_t[256];
    // Compute multiplier.  Concisely, it is:
    //     pow(kMult, (window_size - 1)) % kBase,
    // but we compute the power in integer form.
    uint32_t multiplier = 1;
    for (int i = 0; i < window_size - 1; ++i) {
      multiplier =
          RollingHashUtil::ModBase(multiplier * RollingHashUtil::kMult);
    }
    // For each character removed_byte, compute
    //     remove_table_[removed_byte] ==
    //         (- (removed_byte * pow(kMult, (window_size - 1)))) % kBase
    // where the power operator "pow" is taken in integer form.
    //
    // If you take a hash value fp representing the hash of
    //     buffer[0] ... buffer[window_size - 1]
    // and add the value of remove_table_[buffer[0]] to it, the result will be
    // a partial hash value for
    //     buffer[1] ... buffer[window_size - 1]
    // that is to say, it no longer includes buffer[0].
    //
    // The following byte at buffer[window_size] can then be merged with this
    // partial hash value to arrive quickly at the hash value for a window that
    // has advanced by one byte, to
    //     buffer[1] ... buffer[window_size]
    // In fact, that is precisely what happens in UpdateHash, above.
    uint32_t byte_times_multiplier = 0;
    for (int removed_byte = 0; removed_byte < 256; ++removed_byte) {
      new_remove_table[removed_byte] =
          RollingHashUtil::FindModBaseInverse(byte_times_multiplier);
      // Iteratively adding the multiplier in this loop is equivalent to
      // computing (removed_byte * multiplier), and is faster
      byte_times_multiplier =
          RollingHashUtil::ModBase(byte_times_multiplier + multiplier);
    }
    remove_table_ = new_remove_table;
  }
}

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_ROLLING_HASH_H_
