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
#include "instruction_map.h"
#include <string.h>  // memset
#include "addrcache.h"
#include "vcdiff_defs.h"

namespace open_vcdiff {

// VCDiffInstructionMap members and methods

VCDiffInstructionMap* VCDiffInstructionMap::default_instruction_map = NULL;

VCDiffInstructionMap* VCDiffInstructionMap::GetDefaultInstructionMap() {
  if (!default_instruction_map) {
    default_instruction_map = new VCDiffInstructionMap(
        VCDiffCodeTableData::kDefaultCodeTableData,
        VCDiffAddressCache::DefaultLastMode());
  }
  return default_instruction_map;
}

static unsigned char FindMaxSize(
    const unsigned char size_array[VCDiffCodeTableData::kCodeTableSize]) {
  unsigned char max_size = size_array[0];
  for (int i = 1; i < VCDiffCodeTableData::kCodeTableSize; ++i) {
    if (size_array[i] > max_size) {
      max_size = size_array[i];
    }
  }
  return max_size;
}

static void ClearSizeOpcodeArray(int length, OpcodeOrNone* array) {
  for (int i = 0; i < length; ++i) {
    array[i] = kNoOpcode;
  }
}

static OpcodeOrNone* NewSizeOpcodeArray(int length) {
  OpcodeOrNone* array = new OpcodeOrNone[length];
  ClearSizeOpcodeArray(length, array);
  return array;
}

VCDiffInstructionMap::FirstInstructionMap::FirstInstructionMap(
    int num_insts_and_modes,
    int max_size_1)
    : num_instruction_type_modes_(num_insts_and_modes),
      max_size_1_(max_size_1) {
  first_opcodes_ = new OpcodeOrNone*[num_instruction_type_modes_];
  for (int i = 0; i < num_instruction_type_modes_; ++i) {
    // There must be at least (max_size_1_ + 1) elements in first_opcodes_
    // because the element first_opcodes[max_size_1_] will be referenced.
    first_opcodes_[i] = NewSizeOpcodeArray(max_size_1_ + 1);
  }
}

VCDiffInstructionMap::FirstInstructionMap::~FirstInstructionMap() {
  for (int i = 0; i < num_instruction_type_modes_; ++i) {
    delete[] first_opcodes_[i];
  }
  delete[] first_opcodes_;
}

VCDiffInstructionMap::SecondInstructionMap::SecondInstructionMap(
    int num_insts_and_modes,
    int max_size_2)
    : num_instruction_type_modes_(num_insts_and_modes),
      max_size_2_(max_size_2) {
  memset(second_opcodes_, 0, sizeof(second_opcodes_));
}


VCDiffInstructionMap::SecondInstructionMap::~SecondInstructionMap() {
  for (int opcode = 0; opcode < VCDiffCodeTableData::kCodeTableSize; ++opcode) {
    if (second_opcodes_[opcode] != NULL) {
      for (int inst_mode = 0;
           inst_mode < num_instruction_type_modes_;
           ++inst_mode) {
        // No need to check for NULL
        delete[] second_opcodes_[opcode][inst_mode];
      }
      delete[] second_opcodes_[opcode];
    }
  }
}

void VCDiffInstructionMap::SecondInstructionMap::Add(
    unsigned char first_opcode,
    unsigned char inst,
    unsigned char size,
    unsigned char mode,
    unsigned char second_opcode) {
  OpcodeOrNone**& inst_mode_array = second_opcodes_[first_opcode];
  if (!inst_mode_array) {
    inst_mode_array = new OpcodeOrNone*[num_instruction_type_modes_];
    memset(inst_mode_array,
           0,
           num_instruction_type_modes_ * sizeof(inst_mode_array[0]));
  }
  OpcodeOrNone*& size_array = inst_mode_array[inst + mode];
  if (!size_array) {
    // There must be at least (max_size_2_ + 1) elements in size_array
    // because the element size_array[max_size_2_] will be referenced.
    size_array = NewSizeOpcodeArray(max_size_2_ + 1);
  }
  if (size_array[size] == kNoOpcode) {
    size_array[size] = second_opcode;
  }
}

OpcodeOrNone VCDiffInstructionMap::SecondInstructionMap::Lookup(
    unsigned char first_opcode,
    unsigned char inst,
    unsigned char size,
    unsigned char mode) const {
  if (size > max_size_2_) {
    return kNoOpcode;
  }
  const OpcodeOrNone* const * const inst_mode_array =
    second_opcodes_[first_opcode];
  if (!inst_mode_array) {
    return kNoOpcode;
  }
  int inst_mode = (inst == VCD_COPY) ? (inst + mode) : inst;
  const OpcodeOrNone* const size_array = inst_mode_array[inst_mode];
  if (!size_array) {
    return kNoOpcode;
  }
  return size_array[size];
}

// Because a constructor should never fail, the caller must already
// have run ValidateCodeTable() against the code table data.
//
VCDiffInstructionMap::VCDiffInstructionMap(
    const VCDiffCodeTableData& code_table_data,
    unsigned char max_mode)
    : first_instruction_map_(VCD_LAST_INSTRUCTION_TYPE + max_mode + 1,
                             FindMaxSize(code_table_data.size1)),
      second_instruction_map_(VCD_LAST_INSTRUCTION_TYPE + max_mode + 1,
                              FindMaxSize(code_table_data.size2)) {
  // First pass to fill up first_instruction_map_
  for (int opcode = 0; opcode < VCDiffCodeTableData::kCodeTableSize; ++opcode) {
    if (code_table_data.inst2[opcode] == VCD_NOOP) {
      // Single instruction.  If there is more than one opcode for the same
      // inst, mode, and size, then the lowest-numbered opcode will always
      // be used by the encoder, because of the descending loop.
      first_instruction_map_.Add(code_table_data.inst1[opcode],
                                 code_table_data.size1[opcode],
                                 code_table_data.mode1[opcode],
                                 static_cast<unsigned char>(opcode));
    } else if (code_table_data.inst1[opcode] == VCD_NOOP) {
      // An unusual case where inst1 == NOOP and inst2 == ADD, RUN, or COPY.
      // This is valid under the standard, but unlikely to be used.
      // Add it to the first instruction map as if inst1 and inst2 were swapped.
      first_instruction_map_.Add(code_table_data.inst2[opcode],
                                 code_table_data.size2[opcode],
                                 code_table_data.mode2[opcode],
                                 static_cast<unsigned char>(opcode));
    }
  }
  // Second pass to fill up second_instruction_map_ (depends on first pass)
  for (int opcode = 0; opcode < VCDiffCodeTableData::kCodeTableSize; ++opcode) {
    if ((code_table_data.inst1[opcode] != VCD_NOOP) &&
        (code_table_data.inst2[opcode] != VCD_NOOP)) {
      // Double instruction.  Find the corresponding single instruction opcode
      const OpcodeOrNone single_opcode =
          LookupFirstOpcode(code_table_data.inst1[opcode],
                            code_table_data.size1[opcode],
                            code_table_data.mode1[opcode]);
      if (single_opcode == kNoOpcode) continue;  // No single opcode found
      second_instruction_map_.Add(static_cast<unsigned char>(single_opcode),
                                  code_table_data.inst2[opcode],
                                  code_table_data.size2[opcode],
                                  code_table_data.mode2[opcode],
                                  static_cast<unsigned char>(opcode));
    }
  }
}

};  // namespace open_vcdiff
