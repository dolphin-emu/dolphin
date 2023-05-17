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
//
// A wrapper for the adler32() function from zlib.  This can be replaced
// with another checksum implementation if desired.

#ifndef OPEN_VCDIFF_CHECKSUM_H_
#define OPEN_VCDIFF_CHECKSUM_H_

#include "config.h"
#include "zlib/zlib_old.h"

#ifdef __MINGW32__
#include <stddef.h>
#endif

namespace open_vcdiff {

typedef uLong VCDChecksum;

const VCDChecksum kNoPartialChecksum = 0;

inline VCDChecksum ComputeAdler32(const char* buffer,
                                  size_t size) {
  return adler32_old(kNoPartialChecksum,
                 reinterpret_cast<const Bytef*>(buffer),
                 static_cast<uInt>(size));
}

inline VCDChecksum UpdateAdler32(VCDChecksum partial_checksum,
                                 const char* buffer,
                                 size_t size) {
  return adler32_old(partial_checksum,
                 reinterpret_cast<const Bytef*>(buffer),
                 static_cast<uInt>(size));
}

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_CHECKSUM_H_
