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
// codetable.cc:
//     Classes to implement the Code Table
//     described in sections 5.5, 5.6 and 7 of
//     RFC 3284 - The VCDIFF Generic Differencing and Compression Data Format.
//     The RFC text can be found at http://www.faqs.org/rfcs/rfc3284.html

#include "config.h"
#include "addrcache.h"
#include "codetable.h"
#include "logging.h"
#include "vcdiff_defs.h"  // VCD_MAX_MODES

namespace open_vcdiff {

const char* VCDiffInstructionName(VCDiffInstructionType inst) {
  switch (inst) {
    case VCD_NOOP:
      return "NOOP";
    case VCD_ADD:
      return "ADD";
    case VCD_RUN:
      return "RUN";
    case VCD_COPY:
      return "COPY";
    default:
      VCD_ERROR << "Unexpected instruction type " << inst << VCD_ENDL;
      return "";
  }
}

// This is the default code table defined in the RFC, section 5.6.
// Using a static struct means that the compiler will do the work of
// laying out the values in memory rather than having to use loops to do so
// at runtime.  The letters "N", "A", "R", and "C" are defined as VCD_NOOP,
// VCD_ADD, VCD_RUN, and VCD_COPY respectively (see the definition of
// struct VCDiffCodeTableData), which allows for a compact
// representation of the code table data.
//
const VCDiffCodeTableData VCDiffCodeTableData::kDefaultCodeTableData =
    // inst1
    { { R,  // opcode 0
        A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 1-18
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 19-34
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 35-50
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 51-66
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 67-82
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 83-98
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 99-114
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 115-130
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 131-146
        C, C, C, C, C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 147-162
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 163-174
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 175-186
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 187-198
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 199-210
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 211-222
        A, A, A, A, A, A, A, A, A, A, A, A,  // opcodes 223-234
        A, A, A, A,  // opcodes 235-238
        A, A, A, A,  // opcodes 239-242
        A, A, A, A,  // opcodes 243-246
        C, C, C, C, C, C, C, C, C },  // opcodes 247-255
    // inst2
      { N,  // opcode 0
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 1-18
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 19-34
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 35-50
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 51-66
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 67-82
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 83-98
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 99-114
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 115-130
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 131-146
        N, N, N, N, N, N, N, N, N, N, N, N, N, N, N, N,  // opcodes 147-162
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 163-174
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 175-186
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 187-198
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 199-210
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 211-222
        C, C, C, C, C, C, C, C, C, C, C, C,  // opcodes 223-234
        C, C, C, C,  // opcodes 235-238
        C, C, C, C,  // opcodes 239-242
        C, C, C, C,  // opcodes 243-246
        A, A, A, A, A, A, A, A, A },  // opcodes 247-255
    // size1
      { 0,  // opcode 0
        0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17,  // 1-18
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 19-34
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 35-50
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 51-66
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 67-82
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 83-98
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 99-114
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 115-130
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 131-146
        0, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18,  // 147-162
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 163-174
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 175-186
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 187-198
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 199-210
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 211-222
        1, 1, 1, 2, 2, 2, 3, 3, 3, 4, 4, 4,  // opcodes 223-234
        1, 2, 3, 4,  // opcodes 235-238
        1, 2, 3, 4,  // opcodes 239-242
        1, 2, 3, 4,  // opcodes 243-246
        4, 4, 4, 4, 4, 4, 4, 4, 4 },  // opcodes 247-255
    // size2
      { 0,  // opcode 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 1-18
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 19-34
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 35-50
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 51-66
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 67-82
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 83-98
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 99-114
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 115-130
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 131-146
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 147-162
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 163-174
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 175-186
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 187-198
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 199-210
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 211-222
        4, 5, 6, 4, 5, 6, 4, 5, 6, 4, 5, 6,  // opcodes 223-234
        4, 4, 4, 4,  // opcodes 235-238
        4, 4, 4, 4,  // opcodes 239-242
        4, 4, 4, 4,  // opcodes 243-246
        1, 1, 1, 1, 1, 1, 1, 1, 1 },  // opcodes 247-255
    // mode1
      { 0,  // opcode 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 1-18
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 19-34
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // opcodes 35-50
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // opcodes 51-66
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // opcodes 67-82
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  // opcodes 83-98
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  // opcodes 99-114
        6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6, 6,  // opcodes 115-130
        7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7,  // opcodes 131-146
        8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8, 8,  // opcodes 147-162
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 163-174
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 175-186
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 187-198
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 199-210
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 211-222
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 223-234
        0, 0, 0, 0,  // opcodes 235-238
        0, 0, 0, 0,  // opcodes 239-242
        0, 0, 0, 0,  // opcodes 243-246
        0, 1, 2, 3, 4, 5, 6, 7, 8 },  // opcodes 247-255
    // mode2
      { 0,  // opcode 0
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 1-18
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 19-34
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 35-50
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 51-66
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 67-82
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 83-98
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 99-114
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 115-130
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 131-146
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 147-162
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,  // opcodes 163-174
        1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1,  // opcodes 175-186
        2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2, 2,  // opcodes 187-198
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3,  // opcodes 199-210
        4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4, 4,  // opcodes 211-222
        5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5,  // opcodes 223-234
        6, 6, 6, 6,  // opcodes 235-238
        7, 7, 7, 7,  // opcodes 239-242
        8, 8, 8, 8,  // opcodes 243-246
        0, 0, 0, 0, 0, 0, 0, 0, 0 } };  // opcodes 247-255

bool VCDiffCodeTableData::ValidateOpcode(int opcode,
                                         unsigned char inst,
                                         unsigned char size,
                                         unsigned char mode,
                                         unsigned char max_mode,
                                         const char* first_or_second) {
  bool no_errors_found = true;
  // Check upper limits of inst and mode.  inst, size, and mode are
  // unsigned, so there is no lower limit on them.
  if (inst > VCD_LAST_INSTRUCTION_TYPE) {
    VCD_ERROR << "VCDiff: Bad code table; opcode " << opcode << " has invalid "
              << first_or_second << " instruction type "
              << static_cast<int>(inst) << VCD_ENDL;
    no_errors_found = false;
  }
  if (mode > max_mode) {
    VCD_ERROR << "VCDiff: Bad code table; opcode " << opcode << " has invalid "
              << first_or_second << " mode "
              << static_cast<int>(mode) << VCD_ENDL;
    no_errors_found = false;
  }
  // A NOOP instruction must have size 0
  // (and mode 0, which is included in the next rule)
  if ((inst == VCD_NOOP) && (size != 0)) {
    VCD_ERROR << "VCDiff: Bad code table; opcode " << opcode << " has "
              << first_or_second << " instruction NOOP with nonzero size "
              << static_cast<int>(size) << VCD_ENDL;
    no_errors_found = false;
  }
  // A nonzero mode can only be used with a COPY instruction
  if ((inst != VCD_COPY) && (mode != 0)) {
    VCD_ERROR << "VCDiff: Bad code table; opcode " << opcode
              << " has non-COPY "
              << first_or_second << " instruction with nonzero mode "
              << static_cast<int>(mode) << VCD_ENDL;
    no_errors_found = false;
  }
  return no_errors_found;
}

// If an error is found while validating, continue to validate the rest
// of the code table so that all validation errors will appear in
// the error log.  Otherwise the user would have to fix a single error
// and then rerun validation to find the next error.
//
bool VCDiffCodeTableData::Validate(unsigned char max_mode) const {
  const int kNumberOfTypesAndModes = VCD_LAST_INSTRUCTION_TYPE + max_mode + 1;
  bool hasOpcodeForTypeAndMode[VCD_LAST_INSTRUCTION_TYPE + VCD_MAX_MODES];
  bool no_errors_found = true;
  for (int i = 0; i < kNumberOfTypesAndModes; ++i) {
    hasOpcodeForTypeAndMode[i] = false;
  }
  for (int i = 0; i < kCodeTableSize; ++i) {
    no_errors_found =
        ValidateOpcode(i, inst1[i], size1[i], mode1[i], max_mode, "first")
        && no_errors_found;  // use as 2nd operand to avoid short-circuit
    no_errors_found =
        ValidateOpcode(i, inst2[i], size2[i], mode2[i], max_mode, "second")
        && no_errors_found;
    // A valid code table must have an opcode to encode every possible
    // combination of inst and mode with size=0 as its first instruction,
    // and NOOP as its second instruction.  If this condition fails,
    // then there exists a set of input instructions that cannot be encoded.
    if ((size1[i] == 0) &&
        (inst2[i] == VCD_NOOP) &&
        ((static_cast<int>(inst1[i]) + static_cast<int>(mode1[i]))
            < kNumberOfTypesAndModes)) {
      hasOpcodeForTypeAndMode[inst1[i] + mode1[i]] = true;
    }
  }
  for (int i = 0; i < kNumberOfTypesAndModes; ++i) {
    if (i == VCD_NOOP) continue;
    if (!hasOpcodeForTypeAndMode[i])  {
      if (i >= VCD_COPY) {
        VCD_ERROR << "VCDiff: Bad code table; there is no opcode for inst "
                     "COPY, size 0, mode " << (i - VCD_COPY) << VCD_ENDL;
      } else {
        VCD_ERROR << "VCDiff: Bad code table; there is no opcode for inst "
            << VCDiffInstructionName(static_cast<VCDiffInstructionType>(i))
            << ", size 0,  mode 0" << VCD_ENDL;
      }
      no_errors_found = false;
    }
  }
  return no_errors_found;
}

bool VCDiffCodeTableData::Validate() const {
  return Validate(VCDiffAddressCache::DefaultLastMode());
}

}  // namespace open_vcdiff
