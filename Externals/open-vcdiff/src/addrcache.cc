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
// Implementation of the Address Cache and Address Encoding
// algorithms described in sections 5.1 - 5.4 of RFC 3284 -
// The VCDIFF Generic Differencing and Compression Data Format.
// The RFC text can be found at http://www.faqs.org/rfcs/rfc3284.html
//
// Assumptions:
//   * The VCDAddress type is large enough to hold any offset within
//     the source and target windows.  The limit (for int32_t) is 2^31-1 bytes.
//     The source (dictionary) should not approach this size limit;
//     to compress a target file that is larger than
//     INT_MAX - (dictionary size) bytes, the encoder must
//     break it up into multiple target windows.

#include "config.h"
#include "addrcache.h"
#include "logging.h"
#include "varint_bigendian.h"
#include "vcdiff_defs.h"  // RESULT_ERROR

namespace open_vcdiff {

// The constructor does not initialize near_addresses_ and same_addresses_.
// Therefore, Init() must be called before any other method can be used.
//
// Arguments:
//   near_cache_size: Size of the NEAR cache (number of 4-byte integers)
//   same_cache_size: Size of the SAME cache (number of blocks of
//                                            256 4-byte integers per block)
//     Because the mode is expressed as a byte value,
//     near_cache_size + same_cache_size should not exceed 254.
//
VCDiffAddressCache::VCDiffAddressCache(unsigned char near_cache_size,
                                       unsigned char same_cache_size)
    : near_cache_size_(near_cache_size),
      same_cache_size_(same_cache_size),
      next_slot_(0) { }

VCDiffAddressCache::VCDiffAddressCache()
    : near_cache_size_(kDefaultNearCacheSize),
      same_cache_size_(kDefaultSameCacheSize),
      next_slot_(0) { }

// Sets up data structures needed to call other methods.  Operations that may
// fail at runtime (for example, validating the provided near_cache_size_ and
// same_cache_size_ parameters against their maximum allowed values) are
// confined to this routine in order to guarantee that the class constructor
// will never fail.  Other methods (except the destructor) cannot be invoked
// until this method has been called successfully.  After the object has been
// initialized and used, Init() can be called again to reset it to its initial
// state.
//
// Return value: "true" if initialization succeeded, "false" if it failed.
//     No other method except the destructor may be invoked if this function
//     returns false.  The caller is responsible for checking the return value
//     and providing an exit path in case of error.
//
bool VCDiffAddressCache::Init() {
  // The mode is expressed as a byte value, so there is only room for 256 modes,
  // including the two non-cached modes (SELF and HERE).  Do not allow a larger
  // number of modes to be defined.
  if ((near_cache_size_ + same_cache_size_) > VCD_MAX_MODES - 2) {
    VCD_ERROR << "Using near cache size " << static_cast<int>(near_cache_size_)
              << " and same cache size " << static_cast<int>(same_cache_size_)
              << " would exceed maximum number of COPY modes ("
              << VCD_MAX_MODES << ")" << VCD_ENDL;
    return false;
  }
  if (near_cache_size_ > 0) {
    near_addresses_.assign(near_cache_size_, 0);
  }
  if (same_cache_size_ > 0) {
    same_addresses_.assign(same_cache_size_ * 256, 0);
  }
  next_slot_ = 0;  // in case Init() is called a second time to reinit
  return true;
}

// This method will be called whenever an address is calculated for an
// encoded or decoded COPY instruction, and will update the contents
// of the SAME and NEAR caches.  It is vital that the use of
// UpdateCache (called cache_update in the RFC examples) exactly match
// the RFC standard, and that the same caching logic be used in the
// decoder as in the encoder, in order for the decoded addresses to
// match.
//
// Argument:
//   address: This must be a valid address between 0 and
//       (source window size + target window size).  It is assumed that
//       these bounds have been checked before calling UpdateCache.
//
void VCDiffAddressCache::UpdateCache(VCDAddress address) {
  if (near_cache_size_ > 0) {
    near_addresses_[next_slot_] = address;
    next_slot_ = (next_slot_ + 1) % near_cache_size_;
  }
  if (same_cache_size_ > 0) {
    same_addresses_[address % (same_cache_size_ * 256)] = address;
  }
}

// Determines the address mode that yields the most compact encoding
// of the given address value, writes the encoded address into the
// address stream, and returns the mode used.  The most compact encoding
// is found by looking for the numerically lowest encoded address.
// The Init() function must already have been called.
//
// Arguments:
//   address: The address to be encoded.  Must be a non-negative integer
//       between 0 and (here_address - 1).
//   here_address: The current location in the target data (i.e., the
//       position just after the last encoded value.)  Must be non-negative.
//   encoded_addr: Points to an VCDAddress that will be replaced
//       with the encoded representation of address.
//       If WriteAddressAsVarintForMode returns true when passed
//       the return value, then encoded_addr should be written
//       into the delta file as a variable-length integer (Varint);
//       otherwise, it should be written as a byte (unsigned char).
//
// Return value: A mode value between 0 and 255.  The mode will tell
//       how to interpret the next value in the address stream.
//       The values 0 and 1 correspond to SELF and HERE addressing.
//
// The function is guaranteed to succeed unless the conditions on the arguments
// have not been met, in which case a VCD_DFATAL message will be produced,
// 0 will be returned, and *encoded_addr will be replaced with 0.
//
unsigned char VCDiffAddressCache::EncodeAddress(VCDAddress address,
                                                VCDAddress here_address,
                                                VCDAddress* encoded_addr) {
  if (address < 0) {
    VCD_DFATAL << "EncodeAddress was passed a negative address: "
               << address << VCD_ENDL;
    *encoded_addr = 0;
    return 0;
  }
  if (address >= here_address) {
    VCD_DFATAL << "EncodeAddress was called with address (" << address
               << ") < here_address (" << here_address << ")" << VCD_ENDL;
    *encoded_addr = 0;
    return 0;
  }
  // Try using the SAME cache.  This method, if available, always
  // results in the smallest encoding and takes priority over other modes.
  if (same_cache_size() > 0) {
    const VCDAddress same_cache_pos =
        address % (same_cache_size() * 256);
    if (SameAddress(same_cache_pos) == address) {
      // This is the only mode for which an single byte will be written
      // to the address stream instead of a variable-length integer.
      UpdateCache(address);
      *encoded_addr = same_cache_pos % 256;
      return FirstSameMode() +
          static_cast<unsigned char>(same_cache_pos / 256);  // SAME mode
    }
  }

  // Try SELF mode
  unsigned char best_mode = VCD_SELF_MODE;
  VCDAddress best_encoded_address = address;

  // Try HERE mode
  {
    const VCDAddress here_encoded_address = here_address - address;
    if (here_encoded_address < best_encoded_address) {
      best_mode = VCD_HERE_MODE;
      best_encoded_address = here_encoded_address;
    }
  }

  // Try using the NEAR cache
  for (unsigned char i = 0; i < near_cache_size(); ++i) {
    const VCDAddress near_encoded_address = address - NearAddress(i);
    if ((near_encoded_address >= 0) &&
        (near_encoded_address < best_encoded_address)) {
      best_mode = FirstNearMode() + i;
      best_encoded_address = near_encoded_address;
    }
  }

  UpdateCache(address);
  *encoded_addr = best_encoded_address;
  return best_mode;
}

// Increments *byte_pointer and returns the byte it pointed to before the
// increment.  The caller must check bounds to ensure that *byte_pointer
// points to a valid address in memory.
static unsigned char ParseByte(const char** byte_pointer) {
  unsigned char byte_value = static_cast<unsigned char>(**byte_pointer);
  ++(*byte_pointer);
  return byte_value;
}

// Checks the given decoded address for validity.  Returns true if the
// address is valid; otherwise, prints an error message to the log and
// returns false.
static bool IsDecodedAddressValid(VCDAddress decoded_address,
                                  VCDAddress here_address) {
  if (decoded_address < 0) {
    VCD_ERROR << "Decoded address " << decoded_address << " is invalid"
              << VCD_ENDL;
    return false;
  } else if (decoded_address >= here_address) {
    VCD_ERROR << "Decoded address (" << decoded_address
              << ") is beyond location in target file (" << here_address
              << ")" << VCD_ENDL;
    return false;
  }
  return true;
}

// Interprets the next value in the address_stream using the provided mode,
// which may need to access the SAME or NEAR address cache.  Returns the
// decoded address.
// The Init() function must already have been called.
//
// Arguments:
//   here_address: The current location in the source + target data (i.e., the
//       location into which the COPY instruction will copy.)  By definition,
//       all addresses between 0 and (here_address - 1) are valid, and
//       any other address is invalid.
//   mode: A byte value between 0 and (near_cache_size_ + same_cache_size_ + 1)
//       which tells how to interpret the next value in the address stream.
//       The values 0 and 1 correspond to SELF and HERE addressing.
//       The validity of "mode" should already have been checked before
//       calling this function.
//   address_stream: Points to a pointer holding the position
//       in the "Addresses section for COPYs" part of the input data.
//       That section must already have been uncompressed
//       using a secondary decompressor (if necessary.)
//       This is an IN/OUT argument; the value of *address_stream will be
//       incremented by the size of an integer, or (if the SAME cache
//       was used) by the size of a byte (1).
//   address_stream_end: Points to the position just after the end of
//       the address stream buffer.  All addresses between *address_stream
//       and address_stream_end should contain valid address data.
//
// Return value: If the input conditions were met, and the address section
//     of the input data contains properly encoded addresses that match
//     the instructions section, then an integer between 0 and here_address - 1
//     will be returned, representing the address from which data should
//     be copied from the source or target window into the output stream.
//     If an invalid address value is found in address_stream, then
//     RESULT_ERROR will be returned.  If the limit address_stream_end
//     is reached before the address can be decoded, then
//     RESULT_END_OF_DATA will be returned.  If more streamed data
//     is expected, this means that the consumer should block and wait
//     for more data before continuing to decode.  If no more data is expected,
//     this return value signals an error condition.
//
VCDAddress VCDiffAddressCache::DecodeAddress(VCDAddress here_address,
                                             unsigned char mode,
                                             const char** address_stream,
                                             const char* address_stream_end) {
  if (here_address < 0) {
    VCD_DFATAL << "DecodeAddress was passed a negative value"
                  " for here_address: " << here_address << VCD_ENDL;
    return RESULT_ERROR;
  }
  const char* new_address_pos = *address_stream;
  if (new_address_pos >= address_stream_end) {
    return RESULT_END_OF_DATA;
  }
  VCDAddress decoded_address;
  if (IsSameMode(mode)) {
    // SAME mode expects a byte value as the encoded address
    unsigned char encoded_address = ParseByte(&new_address_pos);
    decoded_address = DecodeSameAddress(mode, encoded_address);
  } else {
    // All modes except SAME mode expect a VarintBE as the encoded address
    int32_t encoded_address = VarintBE<int32_t>::Parse(address_stream_end,
                                                       &new_address_pos);
    switch (encoded_address) {
      case RESULT_ERROR:
        VCD_ERROR << "Found invalid variable-length integer "
                     "as encoded address value" << VCD_ENDL;
        return RESULT_ERROR;
      case RESULT_END_OF_DATA:
        return RESULT_END_OF_DATA;
      default:
        break;
    }
    if (IsSelfMode(mode)) {
      decoded_address = DecodeSelfAddress(encoded_address);
    } else if (IsHereMode(mode)) {
      decoded_address = DecodeHereAddress(encoded_address, here_address);
    } else if (IsNearMode(mode)) {
      decoded_address = DecodeNearAddress(mode, encoded_address);
    } else {
      VCD_DFATAL << "Invalid mode value (" << static_cast<int>(mode)
                 << ") passed to DecodeAddress; maximum mode value = "
                 << static_cast<int>(LastMode()) << VCD_ENDL;
      return RESULT_ERROR;
    }
  }
  // Check for an out-of-bounds address (corrupt/malicious data)
  if (!IsDecodedAddressValid(decoded_address, here_address)) {
    return RESULT_ERROR;
  }
  *address_stream = new_address_pos;
  UpdateCache(decoded_address);
  return decoded_address;
}

}  // namespace open_vcdiff
