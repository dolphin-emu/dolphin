// Copyright 2006 The open-vcdiff Authors. All Rights Reserved.
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
//
// Implementation of the Bentley/McIlroy algorithm for finding differences.
// Bentley, McIlroy.  DCC 1999.  Data Compression Using Long Common Strings.
// http://citeseer.ist.psu.edu/555557.html

#ifndef OPEN_VCDIFF_BLOCKHASH_H_
#define OPEN_VCDIFF_BLOCKHASH_H_

#include "config.h"
#include <stddef.h>  // size_t
#include <stdint.h>  // uint32_t
#include <vector>

namespace open_vcdiff {

// A generic hash table which will be used to keep track of byte runs
// of size kBlockSize in both the incrementally processed target data
// and the preprocessed source dictionary.
//
// A custom hash table implementation is used instead of the standard
// hash_map template because we know that there will be exactly one
// entry in the BlockHash corresponding to each kBlockSize bytes
// in the source data, which makes certain optimizations possible:
// * The memory for the hash table and for all hash entries can be allocated
//   in one step rather than incrementally for each insert operation.
// * A single integer can be used to represent both
//   the index of the next hash entry in the chain
//   and the position of the entry within the source data
//   (== kBlockSize * block_number).  This greatly reduces the size
//   of a hash entry.
//
class BlockHash {
 public:
  // Block size as per Bentley/McIlroy; must be a power of two.
  //
  // Using (for example) kBlockSize = 4 guarantees that no match smaller
  // than size 4 will be identified, that some matches having sizes
  // 4, 5, or 6 may be identified, and that all matches
  // having size 7 or greater will be identified (because any string of
  // 7 bytes must contain a complete aligned block of 4 bytes.)
  //
  // Increasing kBlockSize by a factor of two will halve the amount of
  // memory needed for the next block table, and will halve the setup time
  // for a new BlockHash.  However, it also doubles the minimum
  // match length that is guaranteed to be found in FindBestMatch(),
  // so that function will be less effective in finding matches.
  //
  // Computational effort in FindBestMatch (which is the inner loop of
  // the encoding algorithm) will be proportional to the number of
  // matches found, and a low value of kBlockSize will waste time
  // tracking down small matches.  On the other hand, if this value
  // is set too high, no matches will be found at all.
  //
  // It is suggested that different values of kBlockSize be tried against
  // a representative data set to find the best tradeoff between
  // memory/CPU and the effectiveness of FindBestMatch().
  //
  // If you change kBlockSize to a smaller value, please increase
  // kMaxMatchesToCheck accordingly.
  static const int kBlockSize = 16;

  // This class is used to store the best match found by FindBestMatch()
  // and return it to the caller.
  class Match {
   public:
    Match() : size_(0), source_offset_(-1), target_offset_(-1) { }

    void ReplaceIfBetterMatch(size_t candidate_size,
                              int candidate_source_offset,
                              int candidate_target_offset) {
      if (candidate_size > size_) {
        size_ = candidate_size;
        source_offset_ = candidate_source_offset;
        target_offset_ = candidate_target_offset;
      }
    }

    size_t size() const { return size_; }
    int source_offset() const { return source_offset_; }
    int target_offset() const { return target_offset_; }

   private:
     // The size of the best (longest) match passed to ReplaceIfBetterMatch().
    size_t size_;

    // The source offset of the match, including the starting_offset_
    // of the BlockHash for which the match was found.
    int source_offset_;

    // The target offset of the match.  An offset of 0 corresponds to the
    // data at target_start, which is an argument of FindBestMatch().
    int target_offset_;

    // Making these private avoids implicit copy constructor
    // & assignment operator
    Match(const Match&);  // NOLINT
    void operator=(const Match&);
  };

  // A BlockHash is created using a buffer of source data.  The hash table
  // will contain one entry for each kBlockSize-byte block in the
  // source data.
  //
  // See the comments for starting_offset_, below, for a description of
  // the starting_offset argument.  For a hash of source (dictionary) data,
  // starting_offset_ will be zero; for a hash of previously encoded
  // target data, starting_offset_ will be equal to the dictionary size.
  //
  BlockHash(const char* source_data, size_t source_size, int starting_offset);

  ~BlockHash();

  // Initializes the object before use.
  // This method must be called after constructing a BlockHash object,
  // and before any other method may be called.  This is because
  // Init() dynamically allocates hash_table_ and next_block_table_.
  // Returns true if initialization succeeded, or false if an error occurred,
  // in which case no other method except the destructor may then be used
  // on the object.
  //
  // If populate_hash_table is true, then AddAllBlocks() will be called
  // to populate the hash table.  If populate_hash_table is false, then
  // classes that inherit from BlockHash are expected to call AddBlock()
  // to incrementally populate individual blocks of data.
  //
  bool Init(bool populate_hash_table);

  // In the context of the open-vcdiff encoder, BlockHash is used for two
  // purposes: to hash the source (dictionary) data, and to hash
  // the previously encoded target data.  The main differences between
  // a dictionary BlockHash and a target BlockHash are as follows:
  //
  //   1. The best_match->source_offset() returned from FindBestMatch()
  //      for a target BlockHash is computed in the following manner:
  //      the starting offset of the first byte in the target data
  //      is equal to the dictionary size.  FindBestMatch() will add
  //      starting_offset_ to any best_match->source_offset() value it returns,
  //      in order to produce the correct offset value for a target BlockHash.
  //   2. For a dictionary BlockHash, the entire data set is hashed at once
  //      when Init() is called with the parameter populate_hash_table = true.
  //      For a target BlockHash, because the previously encoded target data
  //      includes only the data seen up to the current encoding position,
  //      the data blocks are hashed incrementally as the encoding position
  //      advances, using AddOneIndexHash() and AddAllBlocksThroughIndex().
  //
  // The following two factory functions can be used to create BlockHash
  // objects for each of these two purposes.  Each factory function calls
  // the object constructor and also calls Init().  If an error occurs,
  // NULL is returned; otherwise a valid BlockHash object is returned.
  // Since a dictionary BlockHash is not expected to be modified after
  // initialization, a const object is returned.
  // The caller is responsible for deleting the returned object
  // (using the C++ delete operator) once it is no longer needed.
  static const BlockHash* CreateDictionaryHash(const char* dictionary_data,
                                               size_t dictionary_size);
  static BlockHash* CreateTargetHash(const char* target_data,
                                     size_t target_size,
                                     size_t dictionary_size);

  // This function will be called to add blocks incrementally to the target hash
  // as the encoding position advances through the target data.  It will be
  // called for every kBlockSize-byte block in the target data, regardless
  // of whether the block is aligned evenly on a block boundary.  The
  // BlockHash will only store hash entries for the evenly-aligned blocks.
  //
  void AddOneIndexHash(int index, uint32_t hash_value) {
    if (index == NextIndexToAdd()) {
      AddBlock(hash_value);
    }
  }

  // Calls AddBlock() for each kBlockSize-byte block in the range
  // (last_block_added_ * kBlockSize, end_index), exclusive of the endpoints.
  // If end_index <= the last index added (last_block_added_ * kBlockSize),
  // this function does nothing.
  //
  // A partial block beginning anywhere up to (end_index - 1) is also added,
  // unless it extends outside the end of the source data.  Like AddAllBlocks(),
  // this function computes the hash value for each of the blocks in question
  // from scratch, so it is not a good option if the hash values have already
  // been computed for some other purpose.
  //
  // Example: assume kBlockSize = 4, last_block_added_ = 1, and there are
  // 14 bytes of source data.
  // If AddAllBlocksThroughIndex(9) is invoked, then it will call AddBlock()
  // only for block number 2 (at index 8).
  // If, after that, AddAllBlocksThroughIndex(14) is invoked, it will not call
  // AddBlock() at all, because block 3 (beginning at index 12) would
  // fall outside the range of source data.
  //
  // VCDiffEngine::Encode (in vcdiffengine.cc) uses this function to
  // add a whole range of data to a target hash when a COPY instruction
  // is generated.
  void AddAllBlocksThroughIndex(int end_index);

  // FindBestMatch takes a position within the unencoded target data
  // (target_candidate_start) and the hash value of the kBlockSize bytes
  // beginning at that position (hash_value).  It attempts to find a matching
  // set of bytes within the source (== dictionary) data, expanding
  // the match both below and above the target block.  It cannot expand
  // the match outside the bounds of the source data, or below
  // target_start within the target data, or past
  // the end limit of (target_start + target_length).
  //
  // target_candidate_start is the start of the candidate block within the
  // target data for which a match will be sought, while
  // target_start (which is <= target_candidate_start)
  // is the start of the target data that has yet to be encoded.
  //
  // If a match is found whose size is greater than the size
  // of best_match, this function populates *best_match with the
  // size, source_offset, and target_offset of the match found.
  // best_match->source_offset() will contain the index of the start of the
  // matching source data, plus starting_offset_
  // (see description of starting_offset_ for details);
  // best_match->target_offset() will contain the offset of the match
  // beginning with target_start = offset 0, such that
  //     0 <= best_match->target_offset()
  //              <= (target_candidate_start - target_start);
  // and best_match->size() will contain the size of the match.
  // If no such match is found, this function leaves *best_match unmodified.
  //
  // On calling FindBestMatch(), best_match must
  // point to a valid Match object, and cannot be NULL.
  // The same Match object can be passed
  // when calling FindBestMatch() on a different BlockHash object
  // for the same candidate data block, in order to find
  // the best match possible across both objects.  For example:
  //
  //     open_vcdiff::BlockHash::Match best_match;
  //     uint32_t hash_value =
  //         RollingHash<BlockHash::kBlockSize>::Hash(target_candidate_start);
  //     bh1.FindBestMatch(hash_value,
  //                       target_candidate_start,
  //                       target_start,
  //                       target_length,
  //                       &best_match);
  //     bh2.FindBestMatch(hash_value,
  //                       target_candidate_start,
  //                       target_start,
  //                       target_length,
  //                       &best_match);
  //     if (best_size >= 0) {
  //       // a match was found; its size, source offset, and target offset
  //       // can be found in best_match
  //     }
  //
  // hash_value is passed as a separate parameter from target_candidate_start,
  // (rather than calculated within FindBestMatch) in order to take
  // advantage of the rolling hash, which quickly calculates the hash value
  // of the block starting at target_candidate_start based on
  // the known hash value of the block starting at (target_candidate_start - 1).
  // See vcdiffengine.cc for more details.
  //
  // Example:
  //    kBlockSize: 4
  //    target text: "ANDREW LLOYD WEBBER"
  //                 1^    5^2^         3^
  //    dictionary: "INSURANCE : LLOYDS OF LONDON"
  //                           4^
  //    hashed dictionary blocks:
  //        "INSU", "RANC", "E : ", "LLOY", "DS O", "F LON"
  //
  //    1: target_start (beginning of unencoded data)
  //    2: target_candidate_start (for the block "LLOY")
  //    3: target_length (points one byte beyond the last byte of data.)
  //    4: best_match->source_offset() (after calling FindBestMatch)
  //    5: best_match->target_offset() (after calling FindBestMatch)
  //
  //    Under these conditions, FindBestMatch will find a matching
  //    hashed dictionary block for "LLOY", and will extend the beginning of
  //    this match backwards by one byte, and the end of the match forwards
  //    by one byte, finding that the best match is " LLOYD"
  //    with best_match->source_offset() = 10
  //                                  (offset of " LLOYD" in the source string),
  //         best_match->target_offset() = 6
  //                                  (offset of " LLOYD" in the target string),
  //     and best_match->size() = 6.
  //
  void FindBestMatch(uint32_t hash_value,
                     const char* target_candidate_start,
                     const char* target_start,
                     size_t target_size,
                     Match* best_match) const;

 protected:
  // FindBestMatch() will not process more than this number
  // of matching hash entries.
  //
  // It is necessary to have a limit on the maximum number of matches
  // that will be checked in order to avoid the worst-case performance
  // possible if, for example, all the blocks in the dictionary have
  // the same hash value.  See the unit test SearchStringFindsTooManyMatches
  // for an example of such a case.  The encoder uses a loop in
  // VCDiffEngine::Encode over each target byte, containing a loop in
  // BlockHash::FindBestMatch over the number of matches (up to a maximum
  // of the number of source blocks), containing two loops that extend
  // the match forwards and backwards up to the number of source bytes.
  // Total complexity in the worst case is
  //     O([target size] * source_size_ * source_size_)
  // Placing a limit on the possible number of matches checked changes this to
  //     O([target size] * source_size_ * kMaxMatchesToCheck)
  //
  // In empirical testing on real HTML text, using a block size of 4,
  // the number of true matches per call to FindBestMatch() did not exceed 78;
  // with a block size of 32, the number of matches did not exceed 3.
  //
  // The expected number of true matches scales super-linearly
  // with the inverse of kBlockSize, but here a linear scale is used
  // for block sizes smaller than 32.
  static const int kMaxMatchesToCheck = (kBlockSize >= 32) ? 32 :
                                            (32 * (32 / kBlockSize));

  // Do not skip more than this number of non-matching hash collisions
  // to find the next matching entry in the hash chain.
  static const int kMaxProbes = 16;

  // Internal routine which calculates a hash table size based on kBlockSize and
  // the dictionary_size.  Will return a power of two if successful, or 0 if an
  // internal error occurs.  Some calculations (such as GetHashTableIndex())
  // depend on the table size being a power of two.
  static size_t CalcTableSize(const size_t dictionary_size);

  size_t GetNumberOfBlocks() const {
    return source_size_ / kBlockSize;
  }

  // Use the lowest-order bits of the hash value
  // as the index into the hash table.
  uint32_t GetHashTableIndex(uint32_t hash_value) const {
    return hash_value & hash_table_mask_;
  }

  // The index within source_data_ of the next block
  // for which AddBlock() should be called.
  int NextIndexToAdd() const {
    return (last_block_added_ + 1) * kBlockSize;
  }

  static inline bool TooManyMatches(int* match_counter);

  const char* source_data() { return source_data_; }
  size_t source_size() { return source_size_; }

  // Adds an entry to the hash table for one block of source data of length
  // kBlockSize, starting at source_data_[block_number * kBlockSize],
  // where block_number is always (last_block_added_ + 1).  That is,
  // AddBlock() must be called once for each block in source_data_
  // in increasing order.
  void AddBlock(uint32_t hash_value);

  // Calls AddBlock() for each complete kBlockSize-byte block between
  // source_data_ and (source_data_ + source_size_).  It is equivalent
  // to calling AddAllBlocksThroughIndex(source_data + source_size).
  // This function is called when Init(true) is invoked.
  void AddAllBlocks();

  // Returns true if the contents of the kBlockSize-byte block
  // beginning at block1 are identical to the contents of
  // the block beginning at block2; false otherwise.
  static bool BlockContentsMatch(const char* block1, const char* block2);

  // Compares each machine word of the two (possibly unaligned) blocks, rather
  // than each byte, thus reducing the number of test-and-branch instructions
  // executed.  Returns a boolean (do the blocks match?) rather than
  // the signed byte difference returned by memcmp.
  //
  // BlockContentsMatch will use either this function or memcmp to do its work,
  // depending on which is faster for a particular architecture.
  //
  // For gcc on x86-based architectures, this function has been shown to run
  // about twice as fast as the library function memcmp(), and between five and
  // nine times faster than the assembly instructions (repz and cmpsb) that gcc
  // uses by default for builtin memcmp.  On other architectures, or using
  // other compilers, this function has not shown to be faster than memcmp.
  static bool BlockCompareWords(const char* block1, const char* block2);

  // Finds the first block number within the hashed data
  // that represents a match for the given hash value.
  // Returns -1 if no match was found.
  //
  // Init() must have been called and returned true before using
  // FirstMatchingBlock or NextMatchingBlock.  No check is performed
  // for this condition; the code will crash if this condition is violated.
  //
  // The hash table is initially populated with -1 (not found) values,
  // so if this function is called before the hash table has been populated
  // using AddAllBlocks() or AddBlock(), it will simply return -1
  // for any value of hash_value.
  int FirstMatchingBlock(uint32_t hash_value, const char* block_ptr) const;

  // Given a block number returned by FirstMatchingBlock()
  // or by a previous call to NextMatchingBlock(), returns
  // the next block number that matches the same hash value.
  // Returns -1 if no match was found.
  int NextMatchingBlock(int block_number, const char* block_ptr) const;

  // Inline version of FirstMatchingBlock.  This saves the cost of a function
  // call when this routine is called from within the module.  The external
  // (non-inlined) version is called only by unit tests.
  inline int FirstMatchingBlockInline(uint32_t hash_value,
                                      const char* block_ptr) const;

  // Walk through the hash entry chain, skipping over any false matches
  // (for which the lowest bits of the fingerprints match,
  // but the actual block data does not.)  Returns the block number of
  // the first true match found, or -1 if no true match was found.
  // If block_number is a matching block, the function will return block_number
  // without skipping to the next block.
  int SkipNonMatchingBlocks(int block_number, const char* block_ptr) const;

  // Returns the number of bytes to the left of source_match_start
  // that match the corresponding bytes to the left of target_match_start.
  // Will not examine more than max_bytes bytes, which is to say that
  // the return value will be in the range [0, max_bytes] inclusive.
  static int MatchingBytesToLeft(const char* source_match_start,
                                 const char* target_match_start,
                                 int max_bytes);

  // Returns the number of bytes starting at source_match_end
  // that match the corresponding bytes starting at target_match_end.
  // Will not examine more than max_bytes bytes, which is to say that
  // the return value will be in the range [0, max_bytes] inclusive.
  static int MatchingBytesToRight(const char* source_match_end,
                                  const char* target_match_end,
                                  int max_bytes);

  // The protected functions BlockContentsMatch, FirstMatchingBlock,
  // NextMatchingBlock, MatchingBytesToLeft, and MatchingBytesToRight
  // should be made accessible to unit tests.
  friend class BlockHashTest;

 private:
  const char* const  source_data_;
  const size_t       source_size_;

  // The size of this array is determined using CalcTableSize().  It has at
  // least one element for each kBlockSize-byte block in the source data.
  // GetHashTableIndex() returns an index into this table for a given hash
  // value.  The value of each element of hash_table_ is the lowest block
  // number in the source data whose hash value would return the same value from
  // GetHashTableIndex(), or -1 if there is no matching block.  This value can
  // then be used as an index into next_block_table_ to retrieve the entire set
  // of matching block numbers.
  std::vector<int> hash_table_;

  // An array containing one element for each source block.  Each element is
  // either -1 (== not found) or the index of the next block whose hash value
  // would produce a matching result from GetHashTableIndex().
  std::vector<int> next_block_table_;

  // This vector has the same size as next_block_table_.  For every block number
  // B that is referenced in hash_table_, last_block_table_[B] will contain
  // the maximum block number that has the same GetHashTableIndex() value
  // as block B.  This number may be B itself.  For a block number B' that
  // is not referenced in hash_table_, the value of last_block_table_[B'] is -1.
  // This table is used only while populating the hash table, not while looking
  // up hash values in the table.  Keeping track of the last block number in the
  // chain allows us to construct the block chains as FIFO rather than LIFO
  // lists, so that the match with the lowest index is returned first.  This
  // should result in a more compact encoding because the VCDIFF format favors
  // smaller index values and repeated index values.
  std::vector<int> last_block_table_;

  // Performing a bitwise AND with hash_table_mask_ will produce a value ranging
  // from 0 to the number of elements in hash_table_.
  uint32_t hash_table_mask_;

  // The offset of the first byte of source data (the data at source_data_[0]).
  // For the purpose of computing offsets, the source data and target data
  // are considered to be concatenated -- not literally in a single memory
  // buffer, but conceptually as described in the RFC.
  // The first byte of the previously encoded target data
  // has an offset that is equal to dictionary_size, i.e., just after
  // the last byte of source data.
  // For a hash of source (dictionary) data, starting_offset_ will be zero;
  // for a hash of previously encoded target data, starting_offset_ will be
  // equal to the dictionary size.
  const int starting_offset_;

  // The last index added by AddBlock().  This determines the block number
  // for successive calls to AddBlock(), and is also
  // used to determine the starting block for AddAllBlocksThroughIndex().
  int last_block_added_;

  // Making these private avoids implicit copy constructor & assignment operator
  BlockHash(const BlockHash&);  // NOLINT
  void operator=(const BlockHash&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_BLOCKHASH_H_
