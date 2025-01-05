// Copyright 2007 The open-vcdiff Authors. All Rights Reserved.
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
// Classes to implement the Address Cache and Address Encoding
// algorithms described in sections 5.1 - 5.4 of RFC 3284 -
// The VCDIFF Generic Differencing and Compression Data Format.
// The RFC text can be found at http://www.faqs.org/rfcs/rfc3284.html

#ifndef OPEN_VCDIFF_ADDRCACHE_H_
#define OPEN_VCDIFF_ADDRCACHE_H_

#include "config.h"
#include <vector>
#include "vcdiff_defs.h"  // VCDAddress

namespace open_vcdiff {

// Implements the "same" and "near" caches
// as described in RFC 3284, section 5.  The "near" cache allows
// efficient reuse of one of the last four referenced addresses
// plus a small offset, and the "same" cache allows efficient reuse
// of an exact recent address distinguished by its lowest-order bits.
//
// NOT threadsafe.
//
class VCDiffAddressCache {
 public:
  // The default cache sizes specified in the RFC
  static const unsigned char kDefaultNearCacheSize = 4;
  static const unsigned char kDefaultSameCacheSize = 3;

  VCDiffAddressCache(unsigned char near_cache_size,
                     unsigned char same_cache_size);

  // This version of the constructor uses the default values
  // kDefaultNearCacheSize and kDefaultSameCacheSize.
  VCDiffAddressCache();

  // Initializes the object before use.  This method must be called after
  // constructing a VCDiffAddressCache/ object, before any other method may be
  // called.  This is because Init() validates near_cache_size_ and
  // same_cache_size_ before initializing the same and near caches.  After the
  // object has been initialized and used, Init() can be called again to reset
  // it to its initial state.
  //
  bool Init();

  unsigned char near_cache_size() const { return near_cache_size_; }

  unsigned char same_cache_size() const { return same_cache_size_; }

  // Returns the first mode number that represents one of the NEAR modes.
  // The number of NEAR modes is near_cache_size.  Each NEAR mode refers to
  // an element of the near_addresses_ array, where a recently-referenced
  // address is stored.
  //
  static unsigned char FirstNearMode() {
    return VCD_FIRST_NEAR_MODE;
  }

  // Returns the first mode number that represents one of the SAME modes.
  // The number of SAME modes is same_cache_size.  Each SAME mode refers to
  // a block of 256 elements of the same_addresses_ array; the lowest-order
  // 8 bits of the address are used to find the element of this block that
  // may match the desired address value.
  //
  unsigned char FirstSameMode() const {
    return VCD_FIRST_NEAR_MODE + near_cache_size();
  }

  // Returns the maximum valid mode number, which happens to be
  // the last SAME mode.
  //
  unsigned char LastMode() const {
    return FirstSameMode() + same_cache_size() - 1;
  }

  static unsigned char DefaultLastMode() {
    return VCD_FIRST_NEAR_MODE
        + kDefaultNearCacheSize + kDefaultSameCacheSize - 1;
  }

  // See the definition of enum VCDiffModes in vcdiff_defs.h,
  // as well as section 5.3 of the RFC, for a description of
  // each address mode type (SELF, HERE, NEAR, and SAME).
  static bool IsSelfMode(unsigned char mode) {
    return mode == VCD_SELF_MODE;
  }

  static bool IsHereMode(unsigned char mode) {
    return mode == VCD_HERE_MODE;
  }

  bool IsNearMode(unsigned char mode) const {
    return (mode >= FirstNearMode()) && (mode < FirstSameMode());
  }

  bool IsSameMode(unsigned char mode) const {
    return (mode >= FirstSameMode()) && (mode <= LastMode());
  }

  static VCDAddress DecodeSelfAddress(int32_t encoded_address) {
    return encoded_address;
  }

  static VCDAddress DecodeHereAddress(int32_t encoded_address,
                                      VCDAddress here_address) {
    return here_address - encoded_address;
  }

  VCDAddress DecodeNearAddress(unsigned char mode,
                               int32_t encoded_address) const {
    return NearAddress(mode - FirstNearMode()) + encoded_address;
  }

  VCDAddress DecodeSameAddress(unsigned char mode,
                               unsigned char encoded_address) const {
    return SameAddress(((mode - FirstSameMode()) * 256) + encoded_address);
  }

  // Returns true if, when using the given mode, an encoded address
  // should be written to the delta file as a variable-length integer;
  // returns false if the encoded address should be written
  // as a byte value (unsigned char).
  bool WriteAddressAsVarintForMode(unsigned char mode) const {
    return !IsSameMode(mode);
  }

  // An accessor for an element of the near_addresses_ array.
  // No bounds checking is performed; the caller must ensure that
  // Init() has already been called, and that
  //     0 <= pos < near_cache_size_
  //
  VCDAddress NearAddress(int pos) const {
    return near_addresses_[pos];
  }

  // An accessor for an element of the same_addresses_ array.
  // No bounds checking is performed; the caller must ensure that
  // Init() has already been called, and that
  //     0 <= pos < (same_cache_size_ * 256)
  //
  VCDAddress SameAddress(int pos) const {
    return same_addresses_[pos];
  }

  // This method will be called whenever an address is calculated for an
  // encoded or decoded COPY instruction, and will update the contents
  // of the SAME and NEAR caches.
  //
  void UpdateCache(VCDAddress address);

  // Determines the address mode that yields the most compact encoding
  // of the given address value.  The most compact encoding
  // is found by looking for the numerically lowest encoded address.
  // Sets *encoded_addr to the encoded representation of the address
  // and returns the mode used.
  //
  // The caller should pass the return value to the method
  // WriteAddressAsVarintForMode() to determine whether encoded_addr
  // should be written to the delta file as a variable-length integer
  // or as a byte (unsigned char).
  //
  unsigned char EncodeAddress(VCDAddress address,
                              VCDAddress here_address,
                              VCDAddress* encoded_addr);

  // Interprets the next value in the address_stream using the provided mode,
  // which may need to access the SAME or NEAR address cache.  Returns the
  // decoded address, or one of the following values:
  //   RESULT_ERROR: An invalid address value was found in address_stream.
  //   RESULT_END_OF_DATA: The limit address_stream_end was reached before
  //       the address could be decoded.  If more streamed data is expected,
  //       this means that the consumer should block and wait for more data
  //       before continuing to decode.  If no more data is expected, this
  //       return value signals an error condition.
  //
  // If successful, *address_stream will be incremented past the decoded address
  // position.  If RESULT_ERROR or RESULT_END_OF_DATA is returned,
  // then the value of *address_stream will not have changed.
  //
  VCDAddress DecodeAddress(VCDAddress here_address,
                           unsigned char mode,
                           const char** address_stream,
                           const char* address_stream_end);

 private:
  // The number of addresses to be kept in the NEAR cache.
  const unsigned char near_cache_size_;
  // The number of 256-byte blocks to store in the SAME cache.
  const unsigned char same_cache_size_;
  // The next position in the NEAR cache to which an address will be written.
  int next_slot_;
  // NEAR cache contents
  std::vector<VCDAddress> near_addresses_;
  // SAME cache contents
  std::vector<VCDAddress> same_addresses_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffAddressCache(const VCDiffAddressCache&);  // NOLINT
  void operator=(const VCDiffAddressCache&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_ADDRCACHE_H_
