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
#include "vcdiffengine.h"
#include <stdint.h>  // uint32_t
#include <string.h>  // memcpy
#include "blockhash.h"
#include "google/codetablewriter_interface.h"
#include "logging.h"
#include "rolling_hash.h"

namespace open_vcdiff {

VCDiffEngine::VCDiffEngine(const char* dictionary, size_t dictionary_size)
    // If dictionary_size == 0, then dictionary could be NULL.  Guard against
    // using a NULL value.
    : dictionary_((dictionary_size > 0) ? new char[dictionary_size] : ""),
      dictionary_size_(dictionary_size),
      hashed_dictionary_(NULL) {
  if (dictionary_size > 0) {
    memcpy(const_cast<char*>(dictionary_), dictionary, dictionary_size);
  }
}

VCDiffEngine::~VCDiffEngine() {
  delete hashed_dictionary_;
  if (dictionary_size_ > 0) {
    delete[] dictionary_;
  }
}

bool VCDiffEngine::Init() {
  if (hashed_dictionary_) {
    VCD_DFATAL << "Init() called twice for same VCDiffEngine object"
               << VCD_ENDL;
    return false;
  }
  hashed_dictionary_ = BlockHash::CreateDictionaryHash(dictionary_,
                                                       dictionary_size());
  if (!hashed_dictionary_) {
    VCD_DFATAL << "Creation of dictionary hash failed" << VCD_ENDL;
    return false;
  }
  RollingHash<BlockHash::kBlockSize>::Init();
  return true;
}

// This helper function tries to find an appropriate match within
// hashed_dictionary_ for the block starting at the current target position.
// If target_hash is not NULL, this function will also look for a match
// within the previously encoded target data.
//
// If a match is found, this function will generate an ADD instruction
// for all unencoded data that precedes the match,
// and a COPY instruction for the match itself; then it returns
// the number of bytes processed by both instructions,
// which is guaranteed to be > 0.
// If no appropriate match is found, the function returns 0.
//
// The first four parameters are input parameters which are passed
// directly to BlockHash::FindBestMatch; please see that function
// for a description of their allowable values.
template<bool look_for_target_matches>
inline size_t VCDiffEngine::EncodeCopyForBestMatch(
    uint32_t hash_value,
    const char* target_candidate_start,
    const char* unencoded_target_start,
    size_t unencoded_target_size,
    const BlockHash* target_hash,
    CodeTableWriterInterface* coder) const {
  // When FindBestMatch() comes up with a match for a candidate block,
  // it will populate best_match with the size, source offset,
  // and target offset of the match.
  BlockHash::Match best_match;

  // First look for a match in the dictionary.
  hashed_dictionary_->FindBestMatch(hash_value,
                                    target_candidate_start,
                                    unencoded_target_start,
                                    unencoded_target_size,
                                    &best_match);
  // If target matching is enabled, then see if there is a better match
  // within the target data that has been encoded so far.
  if (look_for_target_matches) {
    target_hash->FindBestMatch(hash_value,
                               target_candidate_start,
                               unencoded_target_start,
                               unencoded_target_size,
                               &best_match);
  }
  if (!ShouldGenerateCopyInstructionForMatchOfSize(best_match.size())) {
    return 0;
  }
  if (best_match.target_offset() > 0) {
    // Create an ADD instruction to encode all target bytes
    // from the end of the last COPY match, if any, up to
    // the beginning of this COPY match.
    coder->Add(unencoded_target_start, best_match.target_offset());
  }
  coder->Copy(best_match.source_offset(), best_match.size());
  return best_match.target_offset()  // ADD size
       + best_match.size();          // + COPY size
}

// Once the encoder loop has finished checking for matches in the target data,
// this function creates an ADD instruction to encode all target bytes
// from the end of the last COPY match, if any, through the end of
// the target data.  In the worst case, if no matches were found at all,
// this function will create one big ADD instruction
// for the entire buffer of target data.
inline void VCDiffEngine::AddUnmatchedRemainder(
    const char* unencoded_target_start,
    size_t unencoded_target_size,
    CodeTableWriterInterface* coder) const {
  if (unencoded_target_size > 0) {
    coder->Add(unencoded_target_start, unencoded_target_size);
  }
}

template<bool look_for_target_matches>
void VCDiffEngine::EncodeInternal(const char* target_data,
                                  size_t target_size,
                                  OutputStringInterface* diff,
                                  CodeTableWriterInterface* coder) const {
  if (!hashed_dictionary_) {
    VCD_DFATAL << "Internal error: VCDiffEngine::Encode() "
                  "called before VCDiffEngine::Init()" << VCD_ENDL;
    return;
  }
  if (target_size == 0) {
    return;  // Do nothing for empty target
  }
  // Special case for really small input
  if (target_size < static_cast<size_t>(BlockHash::kBlockSize)) {
    AddUnmatchedRemainder(target_data, target_size, coder);
    coder->Output(diff);
    return;
  }
  RollingHash<BlockHash::kBlockSize> hasher;
  BlockHash* target_hash = NULL;
  if (look_for_target_matches) {
    // Check matches against previously encoded target data
    // in this same target window, as well as against the dictionary
    target_hash = BlockHash::CreateTargetHash(target_data,
                                              target_size,
                                              dictionary_size());
    if (!target_hash) {
      VCD_DFATAL << "Instantiation of target hash failed" << VCD_ENDL;
      return;
    }
  }
  const char* const target_end = target_data + target_size;
  const char* const start_of_last_block = target_end - BlockHash::kBlockSize;
  // Offset of next bytes in string to ADD if NOT copied (i.e., not found in
  // dictionary)
  const char* next_encode = target_data;
  // candidate_pos points to the start of the kBlockSize-byte block that may
  // begin a match with the dictionary or previously encoded target data.
  const char* candidate_pos = target_data;
  uint32_t hash_value = hasher.Hash(candidate_pos);
  while (1) {
    const size_t bytes_encoded =
        EncodeCopyForBestMatch<look_for_target_matches>(
            hash_value,
            candidate_pos,
            next_encode,
            (target_end - next_encode),
            target_hash,
            coder);
    if (bytes_encoded > 0) {
      next_encode += bytes_encoded;  // Advance past COPYed data
      candidate_pos = next_encode;
      if (candidate_pos > start_of_last_block) {
        break;  // Reached end of target data
      }
      // candidate_pos has jumped ahead by bytes_encoded bytes, so UpdateHash
      // can't be used to calculate the hash value at its new position.
      hash_value = hasher.Hash(candidate_pos);
      if (look_for_target_matches) {
        // Update the target hash for the ADDed and COPYed data
        target_hash->AddAllBlocksThroughIndex(
            static_cast<int>(next_encode - target_data));
      }
    } else {
      // No match, or match is too small to be worth a COPY instruction.
      // Move to the next position in the target data.
      if ((candidate_pos + 1) > start_of_last_block) {
        break;  // Reached end of target data
      }
      if (look_for_target_matches) {
        target_hash->AddOneIndexHash(
            static_cast<int>(candidate_pos - target_data),
            hash_value);
      }
      hash_value = hasher.UpdateHash(hash_value,
                                     candidate_pos[0],
                                     candidate_pos[BlockHash::kBlockSize]);
      ++candidate_pos;
    }
  }
  AddUnmatchedRemainder(next_encode, target_end - next_encode, coder);
  coder->Output(diff);
  delete target_hash;
}

void VCDiffEngine::Encode(const char* target_data,
                          size_t target_size,
                          bool look_for_target_matches,
                          OutputStringInterface* diff,
                          CodeTableWriterInterface* coder) const {
  if (look_for_target_matches) {
    EncodeInternal<true>(target_data, target_size, diff, coder);
  } else {
    EncodeInternal<false>(target_data, target_size, diff, coder);
  }
}

}  // namespace open_vcdiff
