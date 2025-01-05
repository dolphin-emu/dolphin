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
// There are two different representations of a Code Table's contents:
// VCDiffCodeTableData is the same as the format given in section 7
// of the RFC, and is used for transmission and decoding.  However,
// on the encoding side, it is useful to have a representation that
// can map efficiently from delta instructions to opcodes:
// VCDiffInstructionMap.  A VCDiffInstructionMap is constructed
// using a VCDiffCodeTableData.  For a custom code table, it is recommended
// that the VCDiffCodeTableData be defined as a static struct and that the
// VCDiffInstructionMap be a static pointer that gets initialized only once.

#ifndef OPEN_VCDIFF_INSTRUCTION_MAP_H_
#define OPEN_VCDIFF_INSTRUCTION_MAP_H_

#include "config.h"
#include "codetable.h"
#include "vcdiff_defs.h"

namespace open_vcdiff {

// An alternate representation of the data in a VCDiffCodeTableData that
// optimizes for fast encoding, that is, for taking a delta instruction
// inst (also known as instruction type), size, and mode and arriving at
// the corresponding opcode.
//
class VCDiffInstructionMap {
 public:
  // Create a VCDiffInstructionMap from the information in code_table_data.
  // Does not save a pointer to code_table_data after using its contents
  // to create the instruction->opcode mappings.  The caller *must* have
  // verified that code_table_data->Validate() returned true before
  // attempting to use this constructor.
  // max_mode is the maximum value for the mode of a COPY instruction.
  //
  VCDiffInstructionMap(const VCDiffCodeTableData& code_table_data,
                       unsigned char max_mode);

  static VCDiffInstructionMap* GetDefaultInstructionMap();

  // Finds an opcode that has the given inst, size, and mode for its first
  // instruction  and NOOP for its second instruction (or vice versa.)
  // Returns kNoOpcode if the code table does not have any matching
  // opcode. Otherwise, returns an opcode value between 0 and 255.
  //
  // If this function returns kNoOpcode for size > 0, the caller will
  // usually want to try again with size == 0 to find an opcode that
  // doesn't have a fixed size value.
  //
  // If this function returns kNoOpcode for size == 0, it is an error condition,
  // because any code table that passed the Validate() check should have a way
  // of expressing all combinations of inst and mode with size=0.
  //
  OpcodeOrNone LookupFirstOpcode(unsigned char inst,
                                 unsigned char size,
                                 unsigned char mode) const {
    return first_instruction_map_.Lookup(inst, size, mode);
  }

  // Given a first opcode (presumed to have been returned by a previous call to
  // lookupFirstOpcode), finds an opcode that has the same first instruction as
  // the first opcode, and has the given inst, size, and mode for its second
  // instruction.
  //
  // If this function returns kNoOpcode for size > 0, the caller will
  // usually want to try again with size == 0 to find an opcode that
  // doesn't have a fixed size value.
  //
  OpcodeOrNone LookupSecondOpcode(unsigned char first_opcode,
                                  unsigned char inst,
                                  unsigned char size,
                                  unsigned char mode) const {
    return second_instruction_map_.Lookup(first_opcode, inst, size, mode);
  }

 private:
  // Data structure used to implement LookupFirstOpcode efficiently.
  //
  class FirstInstructionMap {
   public:
    FirstInstructionMap(int num_insts_and_modes, int max_size_1);
    ~FirstInstructionMap();

    void Add(unsigned char inst,
             unsigned char size,
             unsigned char mode,
             unsigned char opcode) {
      OpcodeOrNone* opcode_slot = &first_opcodes_[inst + mode][size];
      if (*opcode_slot == kNoOpcode) {
        *opcode_slot = opcode;
      }
    }

    // See comments for LookupFirstOpcode, above.
    //
    OpcodeOrNone Lookup(unsigned char inst,
                        unsigned char size,
                        unsigned char mode) const {
      int inst_mode = (inst == VCD_COPY) ? (inst + mode) : inst;
      if (size > max_size_1_) {
        return kNoOpcode;
      }
      // Lookup specific-sized opcode
      return first_opcodes_[inst_mode][size];
    }

   private:
    // The number of possible combinations of inst (a VCDiffInstructionType) and
    // mode.  Since the mode is only used for COPY instructions, this number
    // is not (number of VCDiffInstructionType values) * (number of modes), but
    // rather (number of VCDiffInstructionType values other than VCD_COPY)
    // + (number of COPY modes).
    //
    // Compressing inst and mode into a single integer relies on
    // VCD_COPY being the last instruction type.  The inst+mode values are:
    // 0 (NOOP), 1 (ADD), 2 (RUN), 3 (COPY mode 0), 4 (COPY mode 1), ...
    //
    const int num_instruction_type_modes_;

    // The maximum value of a size1 element in code_table_data
    //
    const int max_size_1_;

    // There are two levels to first_opcodes_:
    // 1) A dynamically-allocated pointer array of size
    //    num_instruction_type_modes_ (one element for each combination of inst
    //    and mode.)  Every element of this array is non-NULL and contains
    //    a pointer to:
    // 2) A dynamically-allocated array of OpcodeOrNone values, with one element
    //    for each possible first instruction size (size1) in the code table.
    //    (In the default code table, for example, the maximum size used is 18,
    //    so these arrays would have 19 elements representing values 0
    //    through 18.)
    //
    OpcodeOrNone** first_opcodes_;

    // Making these private avoids implicit copy constructor
    // and assignment operator
    FirstInstructionMap(const FirstInstructionMap&);  // NOLINT
    void operator=(const FirstInstructionMap&);
  } first_instruction_map_;

  // Data structure used to implement LookupSecondOpcode efficiently.
  //
  class SecondInstructionMap {
   public:
    SecondInstructionMap(int num_insts_and_modes, int max_size_2);
    ~SecondInstructionMap();
    void Add(unsigned char first_opcode,
             unsigned char inst,
             unsigned char size,
             unsigned char mode,
             unsigned char second_opcode);

    // See comments for LookupSecondOpcode, above.
    OpcodeOrNone Lookup(unsigned char first_opcode,
                        unsigned char inst,
                        unsigned char size,
                        unsigned char mode) const;
   private:
    // See the member of the same name in FirstInstructionMap.
    const int num_instruction_type_modes_;

    // The maximum value of a size2 element in code_table_data
    const int max_size_2_;

    // There are three levels to second_opcodes_:
    // 1) A statically-allocated pointer array with one element
    //    for each possible opcode.  Each element can be NULL, or can point to:
    // 2) A dynamically-allocated pointer array of size
    //    num_instruction_type_modes_ (one element for each combination of inst
    //    and mode.)  Each element can be NULL, or can point to:
    // 3) A dynamically-allocated array with one element for each possible
    //    second instruction size in the code table.  (In the default code
    //    table, for example, the maximum size used is 6, so these arrays would
    //    have 7 elements representing values 0 through 6.)
    //
    OpcodeOrNone** second_opcodes_[VCDiffCodeTableData::kCodeTableSize];

    // Making these private avoids implicit copy constructor
    // and assignment operator
    SecondInstructionMap(const SecondInstructionMap&);  // NOLINT
    void operator=(const SecondInstructionMap&);
  } second_instruction_map_;

  static VCDiffInstructionMap* default_instruction_map;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffInstructionMap(const VCDiffInstructionMap&);  // NOLINT
  void operator=(const VCDiffInstructionMap&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_INSTRUCTION_MAP_H_
