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
// Types and value definitions to support the implementation of RFC 3284 -
// The VCDIFF Generic Differencing and Compression Data Format.
// The RFC text can be found at http://www.faqs.org/rfcs/rfc3284.html
// Many of the definitions below reference sections in that text.

#ifndef OPEN_VCDIFF_VCDIFF_DEFS_H_
#define OPEN_VCDIFF_VCDIFF_DEFS_H_

#include "config.h"
#include <limits.h>             // UCHAR_MAX
#include <stdint.h>             // int32_t

namespace open_vcdiff {

enum VCDiffResult {
  RESULT_SUCCESS = 0,
  // Many functions within open-vcdiff return signed integer types,
  // and can also return either of these special negative values:
  //
  // An error occurred while performing the requested operation.
  RESULT_ERROR = -1,
  // The end of available data was reached
  // before the requested operation could be completed.
  RESULT_END_OF_DATA = -2
};

// The delta file header section as described in section 4.1 of the RFC:
//
//    "Each delta file starts with a header section organized as below.
//     Note the convention that square-brackets enclose optional items.
//
//           Header1                                  - byte = 0xD6
//           Header2                                  - byte = 0xC3
//           Header3                                  - byte = 0xC4
//           Header4                                  - byte
//           Hdr_Indicator                            - byte
//           [Secondary compressor ID]                - byte
//           [Length of code table data]              - integer
//           [Code table data]
//
//     The first three Header bytes are the ASCII characters 'V', 'C' and
//     'D' with their most significant bits turned on (in hexadecimal, the
//     values are 0xD6, 0xC3, and 0xC4).  The fourth Header byte is
//     currently set to zero.  In the future, it might be used to indicate
//     the version of Vcdiff."
//
typedef struct DeltaFileHeader {
  unsigned char header1;  // Always 0xD6 ('V' | 0x80)
  unsigned char header2;  // Always 0xC3 ('C' | 0x80)
  unsigned char header3;  // Always 0xC4 ('D' | 0x80)
  unsigned char header4;  // 0x00 for standard format, 'S' if extensions used
  unsigned char hdr_indicator;
} DeltaFileHeader;

// The possible values for the Hdr_Indicator field, as described
// in section 4.1 of the RFC:
//
//    "The Hdr_Indicator byte shows if there is any initialization data
//     required to aid in the reconstruction of data in the Window sections.
//     This byte MAY have non-zero values for either, both, or neither of
//     the two bits VCD_DECOMPRESS and VCD_CODETABLE below:
//
//         7 6 5 4 3 2 1 0
//        +-+-+-+-+-+-+-+-+
//        | | | | | | | | |
//        +-+-+-+-+-+-+-+-+
//                     ^ ^
//                     | |
//                     | +-- VCD_DECOMPRESS
//                     +---- VCD_CODETABLE
//
//     If bit 0 (VCD_DECOMPRESS) is non-zero, this indicates that a
//     secondary compressor may have been used to further compress certain
//     parts of the delta encoding data [...]"
// [Secondary compressors are not supported by open-vcdiff.]
//
//    "If bit 1 (VCD_CODETABLE) is non-zero, this indicates that an
//     application-defined code table is to be used for decoding the delta
//     instructions. [...]"
//
const unsigned char VCD_DECOMPRESS = 0x01;
const unsigned char VCD_CODETABLE = 0x02;

// The possible values for the Win_Indicator field, as described
// in section 4.2 of the RFC:
//
//    "Win_Indicator:
//
//     This byte is a set of bits, as shown:
//
//      7 6 5 4 3 2 1 0
//     +-+-+-+-+-+-+-+-+
//     | | | | | | | | |
//     +-+-+-+-+-+-+-+-+
//                  ^ ^
//                  | |
//                  | +-- VCD_SOURCE
//                  +---- VCD_TARGET
//
//     If bit 0 (VCD_SOURCE) is non-zero, this indicates that a
//     segment of data from the "source" file was used as the
//     corresponding source window of data to encode the target
//     window.  The decoder will use this same source data segment to
//     decode the target window.
//
//     If bit 1 (VCD_TARGET) is non-zero, this indicates that a
//     segment of data from the "target" file was used as the
//     corresponding source window of data to encode the target
//     window.  As above, this same source data segment is used to
//     decode the target window.
//
//     The Win_Indicator byte MUST NOT have more than one of the bits
//     set (non-zero).  It MAY have none of these bits set."
//
const unsigned char VCD_SOURCE = 0x01;
const unsigned char VCD_TARGET = 0x02;
// If this flag is set, the delta window includes an Adler32 checksum
// of the target window data.  Not part of the RFC draft standard.
const unsigned char VCD_CHECKSUM = 0x04;

// The possible values for the Delta_Indicator field, as described
// in section 4.3 of the RFC:
//
//    "Delta_Indicator:
//     This byte is a set of bits, as shown:
//
//      7 6 5 4 3 2 1 0
//     +-+-+-+-+-+-+-+-+
//     | | | | | | | | |
//     +-+-+-+-+-+-+-+-+
//                ^ ^ ^
//                | | |
//                | | +-- VCD_DATACOMP
//                | +---- VCD_INSTCOMP
//                +------ VCD_ADDRCOMP
//
//          VCD_DATACOMP:   bit value 1.
//          VCD_INSTCOMP:   bit value 2.
//          VCD_ADDRCOMP:   bit value 4.
//
//     [...] If the bit VCD_DECOMPRESS (Section 4.1) was on, each of these
//     sections may have been compressed using the specified secondary
//     compressor.  The bit positions 0 (VCD_DATACOMP), 1
//     (VCD_INSTCOMP), and 2 (VCD_ADDRCOMP) respectively indicate, if
//     non-zero, that the corresponding parts are compressed."
// [Secondary compressors are not supported, so open-vcdiff decoding will fail
//  if these bits are not all zero.]
//
const unsigned char VCD_DATACOMP = 0x01;
const unsigned char VCD_INSTCOMP = 0x02;
const unsigned char VCD_ADDRCOMP = 0x04;

// A COPY address has 32 bits, which places a limit
// of 2GB on the maximum combined size of the dictionary plus
// the target window (= the chunk of data to be encoded.)
typedef int32_t VCDAddress;

// The address modes used for COPY instructions, as defined in
// section 5.3 of the RFC.
//
// The first two modes (0 and 1) are defined as SELF (addressing forward
// from the beginning of the source window) and HERE (addressing backward
// from the current position in the source window + previously decoded
// target data.)
//
// After those first two modes, there are a variable number of NEAR modes
// (which take a recently-used address and add a positive offset to it)
// and SAME modes (which match a previously-used address using a "hash" of
// the lowest bits of the address.)  The number of NEAR and SAME modes
// depends on the defined size of the address cache; since this number is
// variable, these modes cannot be specified as enum values.
enum VCDiffModes {
  VCD_SELF_MODE = 0,
  VCD_HERE_MODE = 1,
  VCD_FIRST_NEAR_MODE = 2,
  VCD_MAX_MODES = UCHAR_MAX + 1  // 256
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VCDIFF_DEFS_H_
