// Copyright 2006, 2008 The open-vcdiff Authors. All Rights Reserved.
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

#include "config.h"
#include "blockhash.h"
#include <stdint.h>  // uint32_t
#include <string.h>  // memcpy, memcmp
#include <algorithm>  // std::min
#include "compile_assert.h"
#include "logging.h"
#include "rolling_hash.h"

namespace open_vcdiff {

typedef unsigned long uword_t;  // a machine word                         NOLINT

BlockHash::BlockHash(const char* source_data,
                     size_t source_size,
                     int starting_offset)
    : source_data_(source_data),
      source_size_(source_size),
      hash_table_mask_(0),
      starting_offset_(starting_offset),
      last_block_added_(-1) {
}

BlockHash::~BlockHash() { }

// kBlockSize must be at least 2 to be meaningful.  Since it's a compile-time
// constant, check its value at compile time rather than wasting CPU cycles
// on runtime checks.
VCD_COMPILE_ASSERT(BlockHash::kBlockSize >= 2, kBlockSize_must_be_at_least_2);

// kBlockSize is required to be a power of 2 because multiplication
// (n * kBlockSize), division (n / kBlockSize) and MOD (n % kBlockSize)
// are commonly-used operations.  If kBlockSize is a compile-time
// constant and a power of 2, the compiler can convert these three operations
// into bit-shift (>> or <<) and bitwise-AND (&) operations, which are much
// more efficient than executing full integer multiply, divide, or remainder
// instructions.
VCD_COMPILE_ASSERT((BlockHash::kBlockSize & (BlockHash::kBlockSize - 1)) == 0,
                   kBlockSize_must_be_a_power_of_2);

bool BlockHash::Init(bool populate_hash_table) {
  if (!hash_table_.empty() ||
      !next_block_table_.empty() ||
      !last_block_table_.empty()) {
    VCD_DFATAL << "Init() called twice for same BlockHash object" << VCD_ENDL;
    return false;
  }
  const size_t table_size = CalcTableSize(source_size_);
  if (table_size == 0) {
    VCD_DFATAL << "Error finding table size for source size " << source_size_
               << VCD_ENDL;
    return false;
  }
  // Since table_size is a power of 2, (table_size - 1) is a bit mask
  // containing all the bits below table_size.
  hash_table_mask_ = static_cast<uint32_t>(table_size - 1);
  hash_table_.resize(table_size, -1);
  next_block_table_.resize(GetNumberOfBlocks(), -1);
  last_block_table_.resize(GetNumberOfBlocks(), -1);
  if (populate_hash_table) {
    AddAllBlocks();
  }
  return true;
}

const BlockHash* BlockHash::CreateDictionaryHash(const char* dictionary_data,
                                                 size_t dictionary_size) {
  BlockHash* new_dictionary_hash = new BlockHash(dictionary_data,
                                                 dictionary_size,
                                                 0);
  if (!new_dictionary_hash->Init(/* populate_hash_table = */ true)) {
    delete new_dictionary_hash;
    return NULL;
  } else {
    return new_dictionary_hash;
  }
}

BlockHash* BlockHash::CreateTargetHash(const char* target_data,
                                       size_t target_size,
                                       size_t dictionary_size) {
  BlockHash* new_target_hash = new BlockHash(target_data,
                                             target_size,
                                             static_cast<int>(dictionary_size));
  if (!new_target_hash->Init(/* populate_hash_table = */ false)) {
    delete new_target_hash;
    return NULL;
  } else {
    return new_target_hash;
  }
}

// Returns zero if an error occurs.
size_t BlockHash::CalcTableSize(const size_t dictionary_size) {
  // Overallocate the hash table by making it the same size (in bytes)
  // as the source data.  This is a trade-off between space and time:
  // the empty entries in the hash table will reduce the
  // probability of a hash collision to (sizeof(int) / kblockSize),
  // and so save time comparing false matches.
  const size_t min_size = (dictionary_size / sizeof(int)) + 1;  // NOLINT
  size_t table_size = 1;
  // Find the smallest power of 2 that is >= min_size, and assign
  // that value to table_size.
  while (table_size < min_size) {
    table_size <<= 1;
    // Guard against an infinite loop
    if (table_size <= 0) {
      VCD_DFATAL << "Internal error: CalcTableSize(dictionary_size = "
                 << dictionary_size
                 << "): resulting table_size " << table_size
                 << " is zero or negative" << VCD_ENDL;
      return 0;
    }
  }
  // Check size sanity
  if ((table_size & (table_size - 1)) != 0) {
    VCD_DFATAL << "Internal error: CalcTableSize(dictionary_size = "
               << dictionary_size
               << "): resulting table_size " << table_size
               << " is not a power of 2" << VCD_ENDL;
    return 0;
  }
  // The loop above tries to find the smallest power of 2 that is >= min_size.
  // That value must lie somewhere between min_size and (min_size * 2),
  // except for the case (dictionary_size == 0, table_size == 1).
  if ((dictionary_size > 0) && (table_size > (min_size * 2))) {
    VCD_DFATAL << "Internal error: CalcTableSize(dictionary_size = "
               << dictionary_size
               << "): resulting table_size " << table_size
               << " is too large" << VCD_ENDL;
    return 0;
  }
  return table_size;
}

// If the hash value is already available from the rolling hash,
// call this function to save time.
void BlockHash::AddBlock(uint32_t hash_value) {
  if (hash_table_.empty()) {
    VCD_DFATAL << "BlockHash::AddBlock() called before BlockHash::Init()"
               << VCD_ENDL;
    return;
  }
  // The initial value of last_block_added_ is -1.
  int block_number = last_block_added_ + 1;
  const int total_blocks =
      static_cast<int>(source_size_ / kBlockSize);  // round down
  if (block_number >= total_blocks) {
    VCD_DFATAL << "BlockHash::AddBlock() called"
                  " with block number " << block_number
               << " that is past last block " << (total_blocks - 1)
               << VCD_ENDL;
    return;
  }
  if (next_block_table_[block_number] != -1) {
    VCD_DFATAL << "Internal error in BlockHash::AddBlock(): "
                  "block number = " << block_number
               << ", next block should be -1 but is "
               << next_block_table_[block_number] << VCD_ENDL;
    return;
  }
  const uint32_t hash_table_index = GetHashTableIndex(hash_value);
  const int first_matching_block = hash_table_[hash_table_index];
  if (first_matching_block < 0) {
    // This is the first entry with this hash value
    hash_table_[hash_table_index] = block_number;
    last_block_table_[block_number] = block_number;
  } else {
    // Add this entry at the end of the chain of matching blocks
    const int last_matching_block = last_block_table_[first_matching_block];
    if (next_block_table_[last_matching_block] != -1) {
      VCD_DFATAL << "Internal error in BlockHash::AddBlock(): "
                    "first matching block = " << first_matching_block
                 << ", last matching block = " << last_matching_block
                 << ", next block should be -1 but is "
                 << next_block_table_[last_matching_block] << VCD_ENDL;
      return;
    }
    next_block_table_[last_matching_block] = block_number;
    last_block_table_[first_matching_block] = block_number;
  }
  last_block_added_ = block_number;
}

void BlockHash::AddAllBlocks() {
  AddAllBlocksThroughIndex(static_cast<int>(source_size_));
}

void BlockHash::AddAllBlocksThroughIndex(int end_index) {
  if (end_index > static_cast<int>(source_size_)) {
    VCD_DFATAL << "BlockHash::AddAllBlocksThroughIndex() called"
                  " with index " << end_index
               << " higher than end index  " << source_size_ << VCD_ENDL;
    return;
  }
  const int last_index_added = last_block_added_ * kBlockSize;
  if (end_index <= last_index_added) {
    VCD_DFATAL << "BlockHash::AddAllBlocksThroughIndex() called"
                  " with index " << end_index
               << " <= last index added ( " << last_index_added
               << ")" << VCD_ENDL;
    return;
  }
  if (source_size() < static_cast<size_t>(kBlockSize)) {
    // Exit early if the source data is small enough that it does not contain
    // any blocks.  This avoids negative values of last_legal_hash_index.
    // See: https://github.com/google/open-vcdiff/issues/40
    return;
  }
  int end_limit = end_index;
  // Don't allow reading any indices at or past source_size_.
  // The Hash function extends (kBlockSize - 1) bytes past the index,
  // so leave a margin of that size.
  int last_legal_hash_index = static_cast<int>(source_size() - kBlockSize);
  if (end_limit > last_legal_hash_index) {
    end_limit = last_legal_hash_index + 1;
  }
  const char* block_ptr = source_data() + NextIndexToAdd();
  const char* const end_ptr = source_data() + end_limit;
  while (block_ptr < end_ptr) {
    AddBlock(RollingHash<kBlockSize>::Hash(block_ptr));
    block_ptr += kBlockSize;
  }
}

VCD_COMPILE_ASSERT((BlockHash::kBlockSize % sizeof(uword_t)) == 0,
                   kBlockSize_must_be_a_multiple_of_machine_word_size);

// A recursive template to compare a fixed number
// of (possibly unaligned) machine words starting
// at addresses block1 and block2.  Returns true or false
// depending on whether an exact match was found.
template<int number_of_words>
inline bool CompareWholeWordValues(const char* block1,
                                   const char* block2) {
  return CompareWholeWordValues<1>(block1, block2) &&
         CompareWholeWordValues<number_of_words - 1>(block1 + sizeof(uword_t),
                                                     block2 + sizeof(uword_t));
}

// The base of the recursive template: compare one pair of machine words.
template<>
inline bool CompareWholeWordValues<1>(const char* word1,
                                      const char* word2) {
  uword_t aligned_word1, aligned_word2;
  memcpy(&aligned_word1, word1, sizeof(aligned_word1));
  memcpy(&aligned_word2, word2, sizeof(aligned_word2));
  return aligned_word1 == aligned_word2;
}

// A block must be composed of an integral number of machine words
// (uword_t values.)  This function takes advantage of that fact
// by comparing the blocks as series of (possibly unaligned) word values.
// A word-sized comparison can be performed as a single
// machine instruction.  Comparing words instead of bytes means that,
// on a 64-bit platform, this function will use 8 times fewer test-and-branch
// instructions than a byte-by-byte comparison.  Even with the extra
// cost of the calls to memcpy, this method is still at least twice as fast
// as memcmp (measured using gcc on a 64-bit platform, with a block size
// of 32.)  For blocks with identical contents (a common case), this method
// is over six times faster than memcmp.
inline bool BlockCompareWordsInline(const char* block1, const char* block2) {
  static const size_t kWordsPerBlock = BlockHash::kBlockSize / sizeof(uword_t);
  return CompareWholeWordValues<kWordsPerBlock>(block1, block2);
}

bool BlockHash::BlockCompareWords(const char* block1, const char* block2) {
  return BlockCompareWordsInline(block1, block2);
}

inline bool BlockContentsMatchInline(const char* block1, const char* block2) {
  // Optimize for mismatch in first byte.  Since this function is called only
  // when the hash values of the two blocks match, it is very likely that either
  // the blocks are identical, or else the first byte does not match.
  if (*block1 != *block2) {
    return false;
  }
#ifdef VCDIFF_USE_BLOCK_COMPARE_WORDS
  return BlockCompareWordsInline(block1, block2);
#else  // !VCDIFF_USE_BLOCK_COMPARE_WORDS
  return memcmp(block1, block2, BlockHash::kBlockSize) == 0;
#endif  // VCDIFF_USE_BLOCK_COMPARE_WORDS
}

bool BlockHash::BlockContentsMatch(const char* block1, const char* block2) {
  return BlockContentsMatchInline(block1, block2);
}

inline int BlockHash::SkipNonMatchingBlocks(int block_number,
                                            const char* block_ptr) const {
  int probes = 0;
  while ((block_number >= 0) &&
         !BlockContentsMatchInline(block_ptr,
                                   &source_data_[block_number * kBlockSize])) {
    if (++probes > kMaxProbes) {
      return -1;  // Avoid too much chaining
    }
    block_number = next_block_table_[block_number];
  }
  return block_number;
}

// Init() must have been called and returned true before using
// FirstMatchingBlock or NextMatchingBlock.  No check is performed
// for this condition; the code will crash if this condition is violated.
inline int BlockHash::FirstMatchingBlockInline(uint32_t hash_value,
                                               const char* block_ptr) const {
  return SkipNonMatchingBlocks(hash_table_[GetHashTableIndex(hash_value)],
                               block_ptr);
}

int BlockHash::FirstMatchingBlock(uint32_t hash_value,
                                  const char* block_ptr) const {
  return FirstMatchingBlockInline(hash_value, block_ptr);
}

int BlockHash::NextMatchingBlock(int block_number,
                                 const char* block_ptr) const {
  if (static_cast<size_t>(block_number) >= GetNumberOfBlocks()) {
    VCD_DFATAL << "NextMatchingBlock called for invalid block number "
               << block_number << VCD_ENDL;
    return -1;
  }
  return SkipNonMatchingBlocks(next_block_table_[block_number], block_ptr);
}

// Keep a count of the number of matches found.  This will throttle the
// number of iterations in FindBestMatch.  For example, if the entire
// dictionary is made up of spaces (' ') and the search string is also
// made up of spaces, there will be one match for each block in the
// dictionary.
inline bool BlockHash::TooManyMatches(int* match_counter) {
  ++(*match_counter);
  return (*match_counter) > kMaxMatchesToCheck;
}

// Returns the number of bytes to the left of source_match_start
// that match the corresponding bytes to the left of target_match_start.
// Will not examine more than max_bytes bytes, which is to say that
// the return value will be in the range [0, max_bytes] inclusive.
int BlockHash::MatchingBytesToLeft(const char* source_match_start,
                                   const char* target_match_start,
                                   int max_bytes) {
  const char* source_ptr = source_match_start;
  const char* target_ptr = target_match_start;
  int bytes_found = 0;
  while (bytes_found < max_bytes) {
    --source_ptr;
    --target_ptr;
    if (*source_ptr != *target_ptr) {
      break;
    }
    ++bytes_found;
  }
  return bytes_found;
}

// Returns the number of bytes starting at source_match_end
// that match the corresponding bytes starting at target_match_end.
// Will not examine more than max_bytes bytes, which is to say that
// the return value will be in the range [0, max_bytes] inclusive.
int BlockHash::MatchingBytesToRight(const char* source_match_end,
                                    const char* target_match_end,
                                    int max_bytes) {
  const char* source_ptr = source_match_end;
  const char* target_ptr = target_match_end;
  int bytes_found = 0;
  while ((bytes_found < max_bytes) && (*source_ptr == *target_ptr)) {
    ++bytes_found;
    ++source_ptr;
    ++target_ptr;
  }
  return bytes_found;
}

// No NULL checks are performed on the pointer arguments.  The caller
// must guarantee that none of the arguments is NULL, or a crash will occur.
//
// The vast majority of calls to FindBestMatch enter the loop *zero* times,
// which is to say that most candidate blocks find no matches in the dictionary.
// The important sections for optimization are therefore the code outside the
// loop and the code within the loop conditions.  Keep this to a minimum.
void BlockHash::FindBestMatch(uint32_t hash_value,
                              const char* target_candidate_start,
                              const char* target_start,
                              size_t target_size,
                              Match* best_match) const {
  int match_counter = 0;
  for (int block_number = FirstMatchingBlockInline(hash_value,
                                                   target_candidate_start);
       (block_number >= 0) && !TooManyMatches(&match_counter);
       block_number = NextMatchingBlock(block_number, target_candidate_start)) {
    int source_match_offset = block_number * kBlockSize;
    const int source_match_end = source_match_offset + kBlockSize;

    int target_match_offset =
        static_cast<int>(target_candidate_start - target_start);
    const int target_match_end = target_match_offset + kBlockSize;

    size_t match_size = kBlockSize;
    {
      // Extend match start towards beginning of unencoded data
      const int limit_bytes_to_left = std::min(source_match_offset,
                                               target_match_offset);
      const int matching_bytes_to_left =
          MatchingBytesToLeft(source_data_ + source_match_offset,
                              target_start + target_match_offset,
                              limit_bytes_to_left);
      source_match_offset -= matching_bytes_to_left;
      target_match_offset -= matching_bytes_to_left;
      match_size += matching_bytes_to_left;
    }
    {
      // Extend match end towards end of unencoded data
      const size_t source_bytes_to_right = source_size_ - source_match_end;
      const size_t target_bytes_to_right = target_size - target_match_end;
      const size_t limit_bytes_to_right = std::min(source_bytes_to_right,
                                                   target_bytes_to_right);
      match_size +=
          MatchingBytesToRight(source_data_ + source_match_end,
                               target_start + target_match_end,
                               static_cast<int>(limit_bytes_to_right));
    }
    // Update in/out parameter if the best match found was better
    // than any match already stored in *best_match.
    best_match->ReplaceIfBetterMatch(match_size,
                                     source_match_offset + starting_offset_,
                                     target_match_offset);
  }
}

}  // namespace open_vcdiff
