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

#ifndef OPEN_VCDIFF_DECODETABLE_H_
#define OPEN_VCDIFF_DECODETABLE_H_

#include "config.h"
#include <stddef.h>     // NULL
#include <stdint.h>     // int32_t
#include "codetable.h"  // VCDiffInstructi...
#include "logging.h"
#include "unique_ptr.h" // auto_ptr, unique_ptr

namespace open_vcdiff {

// This class is used by the decoder.  It can use a standard or
// non-standard code table, and will translate the opcodes in the code table
// into delta instructions.
//
// NOT threadsafe.
//
class VCDiffCodeTableReader {
 public:
  // When constructed, the object will be set up to use the default code table.
  // If a non-default code table is to be used, then UseCodeTable()
  // should be called after the VCDiffCodeTableReader has been constructed.
  // In any case, the Init() method must be called before GetNextInstruction()
  // may be used.
  //
  VCDiffCodeTableReader();

  // Sets up a non-standard code table.  The caller
  // may free the memory occupied by the argument code table after
  // passing it to this method, because the argument code table
  // allocates space to store a copy of it.
  // UseCodeTable() may be called either before or after calling Init().
  // Returns true if the code table was accepted, or false if the
  // argument did not appear to be a valid code table.
  //
  bool UseCodeTable(const VCDiffCodeTableData& code_table_data,
                    unsigned char max_mode);

  // Defines the buffer containing the instructions and sizes.
  // This method must be called before GetNextInstruction() may be used.
  // Init() may be called any number of times to reset the state of
  // the object.
  //
  void Init(const char** instructions_and_sizes,
            const char* instructions_and_sizes_end) {
    instructions_and_sizes_ = instructions_and_sizes;
    instructions_and_sizes_end_ = instructions_and_sizes_end;
    last_instruction_start_ = NULL;
    pending_second_instruction_ = kNoOpcode;
    last_pending_second_instruction_ = kNoOpcode;
  }

  // Updates the pointers to the buffer containing the instructions and sizes,
  // but leaves the rest of the reader state intact, so that (for example)
  // any pending second instruction or unread instruction will still be
  // read when requested.  NOTE: UnGetInstruction() will not work immediately
  // after using UpdatePointers(); GetNextInstruction() must be called first.
  //
  void UpdatePointers(const char** instructions_and_sizes,
                      const char* instructions_and_sizes_end) {
    instructions_and_sizes_ = instructions_and_sizes;
    instructions_and_sizes_end_ = instructions_and_sizes_end;
    last_instruction_start_ = *instructions_and_sizes;
    // pending_second_instruction_ is unchanged
    last_pending_second_instruction_ = pending_second_instruction_;
  }

  // Returns the next instruction from the stream of opcodes,
  // or VCD_INSTRUCTION_END_OF_DATA if the end of the opcode stream is reached,
  // or VCD_INSTRUCTION_ERROR if an error occurred.
  // In the first of these cases, increments *instructions_and_sizes_
  // past the values it reads, and populates *size
  // with the corresponding size for the returned instruction;
  // otherwise, the value of *size is undefined, and is not
  // guaranteed to be preserved.
  // If the instruction returned is VCD_COPY, *mode will
  // be populated with the copy mode; otherwise, the value of *mode
  // is undefined, and is not guaranteed to be preserved.
  // Any occurrences of VCD_NOOP in the opcode stream
  // are skipped over and ignored, not returned.
  // If Init() was not called before calling this method, then
  // VCD_INSTRUCTION_ERROR will be returned.
  //
  VCDiffInstructionType GetNextInstruction(int32_t* size, unsigned char* mode);

  // Puts a single instruction back onto the front of the
  // instruction stream.  The next call to GetNextInstruction()
  // will return the same value that was returned by the last
  // call.  Calling UnGetInstruction() more than once before calling
  // GetNextInstruction() will have no additional effect; you can
  // only rewind one instruction.
  //
  void UnGetInstruction() {
    if (last_instruction_start_) {
      if (last_instruction_start_ > *instructions_and_sizes_) {
        VCD_DFATAL << "Internal error: last_instruction_start past end of "
                      "instructions_and_sizes in UnGetInstruction" << VCD_ENDL;
      }
      *instructions_and_sizes_ = last_instruction_start_;
      if ((pending_second_instruction_ != kNoOpcode) &&
          (last_pending_second_instruction_ != kNoOpcode)) {
        VCD_DFATAL << "Internal error: two pending instructions in a row "
                      "in UnGetInstruction" << VCD_ENDL;
      }
      pending_second_instruction_ = last_pending_second_instruction_;
    }
  }

 private:
  // A pointer to the code table.  This is the object that will be used
  // to interpret opcodes in GetNextInstruction().
  const VCDiffCodeTableData* code_table_data_;

  // If the default code table is not being used, then space for the
  // code table data will be allocated using this pointer and freed
  // when the VCDiffCodeTableReader is destroyed.  This will keep the
  // code that uses the object from having to worry about memory
  // management for the non-standard code table, whose contents have
  // been read as part of the encoded data file/stream.
  //
  UNIQUE_PTR<VCDiffCodeTableData> non_default_code_table_data_;

  const char** instructions_and_sizes_;
  const char* instructions_and_sizes_end_;
  const char* last_instruction_start_;
  OpcodeOrNone pending_second_instruction_;
  OpcodeOrNone last_pending_second_instruction_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffCodeTableReader(const VCDiffCodeTableReader&);
  void operator=(const VCDiffCodeTableReader&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_DECODETABLE_H_
