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
// VCDiffCodeTableReader is a class to interpret a stream of opcodes
// as VCDIFF instruction types, based on a VCDiffCodeTableData structure.

#include "config.h"
#include "decodetable.h"
#include "codetable.h"
#include "logging.h"
#include "varint_bigendian.h"
#include "vcdiff_defs.h"

namespace open_vcdiff {

VCDiffCodeTableReader::VCDiffCodeTableReader()
    : code_table_data_(&VCDiffCodeTableData::kDefaultCodeTableData),
      instructions_and_sizes_(NULL),
      instructions_and_sizes_end_(NULL),
      last_instruction_start_(NULL),
      pending_second_instruction_(kNoOpcode),
      last_pending_second_instruction_(kNoOpcode) {
}

bool VCDiffCodeTableReader::UseCodeTable(
    const VCDiffCodeTableData& code_table_data, unsigned char max_mode) {
  if (!code_table_data.Validate(max_mode)) return false;
  if (!non_default_code_table_data_.get()) {
    non_default_code_table_data_.reset(new VCDiffCodeTableData);
  }
  *non_default_code_table_data_ = code_table_data;
  code_table_data_ = non_default_code_table_data_.get();
  return true;
}

VCDiffInstructionType VCDiffCodeTableReader::GetNextInstruction(
    int32_t* size,
    unsigned char* mode) {
  if (!instructions_and_sizes_) {
    VCD_ERROR << "Internal error: GetNextInstruction() called before Init()"
              << VCD_ENDL;
    return VCD_INSTRUCTION_ERROR;
  }
  last_instruction_start_ = *instructions_and_sizes_;
  last_pending_second_instruction_ = pending_second_instruction_;
  unsigned char opcode = 0;
  unsigned char instruction_type = VCD_NOOP;
  int32_t instruction_size = 0;
  unsigned char instruction_mode = 0;
  do {
    if (pending_second_instruction_ != kNoOpcode) {
      // There is a second instruction left over
      // from the most recently processed opcode.
      opcode = static_cast<unsigned char>(pending_second_instruction_);
      pending_second_instruction_ = kNoOpcode;
      instruction_type = code_table_data_->inst2[opcode];
      instruction_size = code_table_data_->size2[opcode];
      instruction_mode = code_table_data_->mode2[opcode];
      break;
    }
    if (*instructions_and_sizes_ >= instructions_and_sizes_end_) {
      // Ran off end of instruction stream
      return VCD_INSTRUCTION_END_OF_DATA;
    }
    opcode = **instructions_and_sizes_;
    if (code_table_data_->inst2[opcode] != VCD_NOOP) {
      // This opcode contains two instructions; process the first one now, and
      // save a pointer to the second instruction, which should be returned
      // by the next call to GetNextInstruction
      pending_second_instruction_ = **instructions_and_sizes_;
    }
    ++(*instructions_and_sizes_);
    instruction_type = code_table_data_->inst1[opcode];
    instruction_size = code_table_data_->size1[opcode];
    instruction_mode = code_table_data_->mode1[opcode];
  // This do-while loop is necessary in case inst1 == VCD_NOOP for an opcode
  // that was actually used in the encoding.  That case is unusual, but it
  // is not prohibited by the standard.
  } while (instruction_type == VCD_NOOP);
  if (instruction_size == 0) {
    // Parse the size as a Varint in the instruction stream.
    switch (*size = VarintBE<int32_t>::Parse(instructions_and_sizes_end_,
                                             instructions_and_sizes_)) {
      case RESULT_ERROR:
        VCD_ERROR << "Instruction size is not a valid variable-length integer"
                  << VCD_ENDL;
        return VCD_INSTRUCTION_ERROR;
      case RESULT_END_OF_DATA:
        UnGetInstruction();  // Rewind to instruction start
        return VCD_INSTRUCTION_END_OF_DATA;
      default:
        break;  // Successfully parsed Varint
    }
  } else {
    *size = instruction_size;
  }
  *mode = instruction_mode;
  return static_cast<VCDiffInstructionType>(instruction_type);
}

};  // namespace open_vcdiff
