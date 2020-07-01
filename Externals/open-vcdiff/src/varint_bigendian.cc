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

#include "config.h"
#include "varint_bigendian.h"
#include <stdint.h>  // int32_t, int64_t
#include <string.h>  // memcpy
#include <string>
#include "logging.h"
#include "google/output_string.h"

namespace open_vcdiff {

template<> const int32_t VarintBE<int32_t>::kMaxVal = 0x7FFFFFFF;
template<> const int64_t VarintBE<int64_t>::kMaxVal = 0x7FFFFFFFFFFFFFFFULL;

// Reads a variable-length integer from **varint_ptr
// and returns it in a fixed-length representation.  Increments
// *varint_ptr by the number of bytes read.  Will not read
// past limit.  Returns RESULT_ERROR if the value parsed
// does not fit in a non-negative signed integer, or if limit is NULL.
// Returns RESULT_END_OF_DATA if address_stream_end is reached
// before the whole integer can be decoded.
//
template <typename SignedIntegerType>
SignedIntegerType VarintBE<SignedIntegerType>::Parse(const char* limit,
                                                     const char** varint_ptr) {
  if (!limit) {
    return RESULT_ERROR;
  }
  SignedIntegerType result = 0;
  for (const char* parse_ptr = *varint_ptr; parse_ptr < limit; ++parse_ptr) {
    result += *parse_ptr & 0x7F;
    if (!(*parse_ptr & 0x80)) {
      *varint_ptr = parse_ptr + 1;
      return result;
    }
    if (result > (kMaxVal >> 7)) {
      // Shifting result by 7 bits would produce a number too large
      // to be stored in a non-negative SignedIntegerType (an overflow.)
      return RESULT_ERROR;
    }
    result = result << 7;
  }
  return RESULT_END_OF_DATA;
}

template <typename SignedIntegerType>
int VarintBE<SignedIntegerType>::EncodeInternal(SignedIntegerType v,
                                                char* varint_buf) {
  if (v < 0) {
    VCD_DFATAL << "Negative value " << v
               << " passed to VarintBE::EncodeInternal,"
                  " which requires non-negative argument" << VCD_ENDL;
    return 0;
  }
  int length = 1;
  char* buf_ptr = &varint_buf[kMaxBytes - 1];
  *buf_ptr = static_cast<char>(v & 0x7F);
  --buf_ptr;
  v >>= 7;
  while (v) {
    *buf_ptr = static_cast<char>((v & 0x7F) | 0x80);  // add continuation bit
    --buf_ptr;
    ++length;
    v >>= 7;
  }
  return length;
}

template <typename SignedIntegerType>
int VarintBE<SignedIntegerType>::Encode(SignedIntegerType v, char* ptr) {
  char varint_buf[kMaxBytes];
  const int length = EncodeInternal(v, varint_buf);
  memcpy(ptr, &varint_buf[kMaxBytes - length], length);
  return length;
}

template <typename SignedIntegerType>
void VarintBE<SignedIntegerType>::AppendToString(SignedIntegerType value,
                                                 string* s) {
  char varint_buf[kMaxBytes];
  const int length = EncodeInternal(value, varint_buf);
  s->append(&varint_buf[kMaxBytes - length], length);
}

template <typename SignedIntegerType>
void VarintBE<SignedIntegerType>::AppendToOutputString(
    SignedIntegerType value,
    OutputStringInterface* output_string) {
  char varint_buf[kMaxBytes];
  const int length = EncodeInternal(value, varint_buf);
  output_string->append(&varint_buf[kMaxBytes - length], length);
}

// Returns the encoding length of the specified value.
template <typename SignedIntegerType>
int VarintBE<SignedIntegerType>::Length(SignedIntegerType v) {
  if (v < 0) {
    VCD_DFATAL << "Negative value " << v
               << " passed to VarintBE::Length,"
                  " which requires non-negative argument" << VCD_ENDL;
    return 0;
  }
  int length = 0;
  do {
    v >>= 7;
    ++length;
  } while (v);
  return length;
}

template class VarintBE<int32_t>;
template class VarintBE<int64_t>;

}  // namespace open_vcdiff
