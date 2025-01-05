// Copyright 2008 The open-vcdiff Authors. All Rights Reserved.
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

#ifndef OPEN_VCDIFF_VARINT_BIGENDIAN_H_
#define OPEN_VCDIFF_VARINT_BIGENDIAN_H_

// Functions for manipulating variable-length integers as described in
// RFC 3284, section 2.  (See http://www.ietf.org/rfc/rfc3284.txt)
// This is the same format used by the Sfio library
// and by the public-domain Sqlite package.
//
// The implementation found in this file contains buffer bounds checks
// (not available in sqlite) and its goal is to improve speed
// by using as few test-and-branch instructions as possible.
//
// The Sqlite format has the refinement that, if a 64-bit value is expected,
// the ninth byte of the varint does not have a continuation bit, but instead
// encodes 8 bits of information.  This allows 64 bits to be encoded compactly
// in nine bytes.  However, that refinement does not appear in the format
// description in RFC 3284, and so it is not used here.  In any case,
// this header file deals only with *signed* integer types, and so a
// "64-bit" integer is allowed to have only 63 significant bits; an additional
// 64th bit would indicate a negative value and therefore an error.
//

#include "config.h"
#include <stdint.h>  // int32_t, int64_t
#include <string>
#include "vcdiff_defs.h"  // RESULT_ERROR

namespace open_vcdiff {

class OutputStringInterface;

// This helper class is needed in order to ensure that
// VarintBE<SignedIntegerType>::kMaxBytes is treated
// as a compile-time constant when it is used as the size
// of a static array.
template <typename SignedIntegerType> class VarintMaxBytes;

// 31 bits of data / 7 bits per byte <= 5 bytes
template<> class VarintMaxBytes<int32_t> {
 public:
  static const int kMaxBytes = 5;
};

// 63 bits of data / 7 bits per byte == 9 bytes
template<> class VarintMaxBytes<int64_t> {
 public:
  static const int kMaxBytes = 9;
};

// Objects of type VarintBE should not be instantiated.  The class is a
// container for big-endian constant values and functions used to parse
// and write a particular signed integer type.
// Example: to parse a 32-bit integer value stored as a big-endian varint, use
//     int32_t value = VarintBE<int32_t>::Parse(&ptr, limit);
// Only 32-bit and 64-bit signed integers (int32_t and int64_t) are supported.
// Using a different type as the template argument will likely result
// in a link-time error for an undefined Parse() or Append() function.
//
template <typename SignedIntegerType>
class VarintBE {  // BE stands for Big-Endian
 public:
  typedef std::string string;

  // The maximum positive value represented by a SignedIntegerType.
  static const SignedIntegerType kMaxVal;

  // Returns the maximum number of bytes needed to store a varint
  // representation of a <SignedIntegerType> value.
  static const int kMaxBytes = VarintMaxBytes<SignedIntegerType>::kMaxBytes;

  // Attempts to parse a big-endian varint from a prefix of the bytes
  // in [ptr,limit-1] and convert it into a signed, non-negative 32-bit
  // integer.  Never reads a character at or beyond limit.
  // If a parsed varint would exceed the maximum value of
  // a <SignedIntegerType>, returns RESULT_ERROR and does not modify *ptr.
  // If parsing a varint at *ptr (without exceeding the capacity of
  // a <SignedIntegerType>) would require reading past limit,
  // returns RESULT_END_OF_DATA and does not modify *ptr.
  // If limit == NULL, returns RESULT_ERROR.
  // If limit < *ptr, returns RESULT_END_OF_DATA.
  static SignedIntegerType Parse(const char* limit, const char** ptr);

  // Returns the encoding length of the specified value.
  static int Length(SignedIntegerType v);

  // Encodes "v" into "ptr" (which points to a buffer of length sufficient
  // to hold "v")and returns the length of the encoding.
  // The value of v must not be negative.
  static int Encode(SignedIntegerType v, char* ptr);

  // Appends the varint representation of "value" to "*s".
  // The value of v must not be negative.
  static void AppendToString(SignedIntegerType value, string* s);

  // Appends the varint representation of "value" to output_string.
  // The value of v must not be negative.
  static void AppendToOutputString(SignedIntegerType value,
                                   OutputStringInterface* output_string);

 private:
  // Encodes "v" into the LAST few bytes of varint_buf (which is a char array
  // of size kMaxBytes) and returns the length of the encoding.
  // The result will be stored in buf[(kMaxBytes - length) : (kMaxBytes - 1)],
  // rather than in buf[0 : length].
  // The value of v must not be negative.
  static int EncodeInternal(SignedIntegerType v, char* varint_buf);

  // These are private to avoid constructing any objects of this type
  VarintBE();
  VarintBE(const VarintBE&);  // NOLINT
  void operator=(const VarintBE&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VARINT_BIGENDIAN_H_
