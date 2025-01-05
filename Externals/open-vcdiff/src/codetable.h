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
// Classes to implement the Code Table
// described in sections 5.5, 5.6 and 7 of
// RFC 3284 - The VCDIFF Generic Differencing and Compression Data Format.
// The RFC text can be found at http://www.faqs.org/rfcs/rfc3284.html

#ifndef OPEN_VCDIFF_CODETABLE_H_
#define OPEN_VCDIFF_CODETABLE_H_

#include "config.h"
#include <stdint.h>             // uint16_t

namespace open_vcdiff {

// The instruction types from section 5.5 (mistakenly labeled 5.4) of the RFC.
//
enum VCDiffInstructionType {
  VCD_NOOP = 0,
  VCD_ADD  = 1,
  VCD_RUN  = 2,
  VCD_COPY = 3,
  VCD_LAST_INSTRUCTION_TYPE = VCD_COPY,
  // The following values are not true instruction types, but rather
  // special condition values for functions that return VCDiffInstructionType.
  VCD_INSTRUCTION_ERROR = 4,
  VCD_INSTRUCTION_END_OF_DATA = 5
};

const char* VCDiffInstructionName(VCDiffInstructionType inst);

// OpcodeOrNone: An opcode is a value between 0-255.  There is not room
// in a single byte to express all these values plus a "no opcode found"
// value.  So use a 16-bit integer to hold either an opcode or kNoOpcode.
//
typedef uint16_t OpcodeOrNone;
const OpcodeOrNone kNoOpcode = 0x100;  // outside the opcode range 0x00 - 0xFF

// struct VCDiffCodeTableData:
//
// A representation of the VCDiff code table as six 256-byte arrays
// as described in Section 7 of RFC 3284.  Each instruction code
// can represent up to two delta instructions, which is why inst,
// size, and mode each appear twice.  Each of the two delta instructions
// has the following three attributes:
// * inst (NOOP, ADD, RUN, or COPY)
// * size (0-255 bytes) of the data to be copied; if this value is zero, then
//   the size will be encoded separately from the instruction code, as a Varint
// * mode (SELF, HERE, NEAR(n), or SAME(n)), only used for COPY instructions
//
// Every valid code table should contain AT LEAST the following instructions:
//     inst1=ADD  size1=0 mode1=X inst2=NOOP size2=X mode2=X
//     inst1=RUN  size1=0 mode1=X inst2=NOOP size2=X mode2=X
//     inst1=COPY size1=0 mode1=N inst2=NOOP size2=X mode2=X (for all N)
// ... where X represents a "don't care" value which will not be read,
// and N stands for every possible COPY mode between 0 and
// ([same cache size] + [here cache size]) inclusive.
// Without these instructions, it will be impossible to guarantee that
// all ADD, RUN, and COPY encoding requests can be fulfilled.
//
struct VCDiffCodeTableData {
  static const int kCodeTableSize = 256;

  static const VCDiffCodeTableData kDefaultCodeTableData;

  // Validates that the data contained in the VCDiffCodeTableData structure
  // does not violate certain assumptions.  Returns true if none of these
  // assumptions are violated, or false if an unexpected value is found.
  // This function should be called on any non-default code table that is
  // received as part of an encoded transmission.
  // max_mode is the maximum value for the mode of a COPY instruction;
  // this is equal to same_cache_size + near_cache_size + 1.
  //
  bool Validate(unsigned char max_mode) const;

  // This version of Validate() assumes that the default address cache sizes
  // are being used, and calculates max_mode based on that assumption.
  bool Validate() const;

  // The names of these elements are taken from RFC 3284 section 5.4
  // (Instruction Codes), which contains the following specification:
  //
  // Each instruction code entry contains six fields, each of which is a single
  // byte with an unsigned value:
  // +-----------------------------------------------+
  // | inst1 | size1 | mode1 | inst2 | size2 | mode2 |
  // +-----------------------------------------------+
  //
  unsigned char inst1[kCodeTableSize];  // from enum VCDiffInstructionType
  unsigned char inst2[kCodeTableSize];  // from enum VCDiffInstructionType
  unsigned char size1[kCodeTableSize];
  unsigned char size2[kCodeTableSize];
  unsigned char mode1[kCodeTableSize];  // from enum VCDiffModes
  unsigned char mode2[kCodeTableSize];  // from enum VCDiffModes

 private:
  // Single-letter abbreviations that make it easier to read
  // the default code table data.
  static const VCDiffInstructionType N = VCD_NOOP;
  static const VCDiffInstructionType A = VCD_ADD;
  static const VCDiffInstructionType R = VCD_RUN;
  static const VCDiffInstructionType C = VCD_COPY;

  static bool ValidateOpcode(int opcode,
                             unsigned char inst,
                             unsigned char size,
                             unsigned char mode,
                             unsigned char max_mode,
                             const char* first_or_second);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_CODETABLE_H_
