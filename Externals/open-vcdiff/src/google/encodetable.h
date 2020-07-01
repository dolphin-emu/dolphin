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

#ifndef OPEN_VCDIFF_ENCODETABLE_H_
#define OPEN_VCDIFF_ENCODETABLE_H_

#include "config.h"
#include <stddef.h>  // size_t
#include <stdint.h>  // int32_t
#include <string>
#include "../addrcache.h"
#include "../checksum.h"
#include "../codetable.h"
#include "codetablewriter_interface.h"

namespace open_vcdiff {

class OutputStringInterface;
class VCDiffInstructionMap;

// The method calls after construction *must* conform
// to the following pattern:
//    {{Add|Copy|Run}* [AddChecksum] Output}*
//
// When Output has been called in this sequence, a complete target window
// (as defined in RFC 3284 section 4.3) will have been appended to
// out (unless no calls to Add, Run, or Copy were made, in which
// case Output will do nothing.)  The output will not be available for use
// until after each call to Output().
//
// NOT threadsafe.
//
class VCDiffCodeTableWriter : public CodeTableWriterInterface {
 public:
  // This constructor uses the default code table.
  // If interleaved is true, the encoder writes each delta file window
  // by interleaving instructions and sizes with their corresponding
  // addresses and data, rather than placing these elements into three
  // separate sections.  This facilitates providing partially
  // decoded results when only a portion of a delta file window
  // is received (e.g. when HTTP over TCP is used as the
  // transmission protocol.)  The interleaved format is
  // not consistent with the VCDIFF draft standard.
  //
  explicit VCDiffCodeTableWriter(bool interleaved);

  // Uses a non-standard code table and non-standard cache sizes.  The caller
  // must guarantee that code_table_data remains allocated for the lifetime of
  // the VCDiffCodeTableWriter object.  Note that this is different from how
  // VCDiffCodeTableReader::UseCodeTable works.  It is assumed that a given
  // encoder will use either the default code table or a statically-defined
  // non-standard code table, whereas the decoder must have the ability to read
  // an arbitrary non-standard code table from a delta file and discard it once
  // the file has been decoded.
  //
  VCDiffCodeTableWriter(bool interleaved,
                        unsigned char near_cache_size,
                        unsigned char same_cache_size,
                        const VCDiffCodeTableData& code_table_data,
                        unsigned char max_mode);

  virtual ~VCDiffCodeTableWriter();

  // Initializes the constructed object for use.
  // This method must be called after a VCDiffCodeTableWriter is constructed
  // and before any of its other methods can be called.  It will return
  // false if there was an error initializing the object, or true if it
  // was successful.  After the object has been initialized and used,
  // Init() can be called again to restore the initial state of the object.
  //
  virtual bool Init(size_t dictionary_size);

  // Write the header (as defined in section 4.1 of the RFC) to *out.
  // This includes information that can be gathered
  // before the first chunk of input is available.
  virtual void WriteHeader(OutputStringInterface* out,
                           VCDiffFormatExtensionFlags format_extensions);

  // Encode an ADD opcode with the "size" bytes starting at data
  virtual void Add(const char* data, size_t size);

  // Encode a COPY opcode with args "offset" (into dictionary) and "size" bytes.
  virtual void Copy(int32_t offset, size_t size);

  // Encode a RUN opcode for "size" copies of the value "byte".
  virtual void Run(size_t size, unsigned char byte);

  virtual void AddChecksum(VCDChecksum checksum) {
    add_checksum_ = true;
    checksum_ = checksum;
  }

  // Appends the encoded delta window to the output
  // string.  The output string is not null-terminated and may contain embedded
  // '\0' characters.
  virtual void Output(OutputStringInterface* out);

  // There should not be any need to output more data
  // since EncodeChunk() encodes a complete target window
  // and there is no end-of-delta-file marker.
  virtual void FinishEncoding(OutputStringInterface* /*out*/) {}

  // Verifies dictionary is compatible with writer.
  virtual bool VerifyDictionary(const char * /*dictionary*/,
                                size_t /*size*/) const;

  // Verifies target chunk is compatible with writer.
  virtual bool VerifyChunk(const char * /*chunk*/, size_t /*size*/) const;

 private:
  typedef std::string string;

  // The maximum value for the mode of a COPY instruction.
  const unsigned char max_mode_;

  // If interleaved is true, sets data_for_add_and_run_ and
  // addresses_for_copy_ to point at instructions_and_sizes_,
  // so that instructions, sizes, addresses and data will be
  // combined into a single interleaved stream.
  // If interleaved is false, sets data_for_add_and_run_ and
  // addresses_for_copy_ to point at their corresponding
  // separate_... strings, so that the three sections will
  // be generated separately from one another.
  //
  void InitSectionPointers(bool interleaved);

  // Determines the best opcode to encode an instruction, and appends
  // or substitutes that opcode and its size into the
  // instructions_and_sizes_ string.
  //
  void EncodeInstruction(VCDiffInstructionType inst,
                         size_t size,
                         unsigned char mode);

  void EncodeInstruction(VCDiffInstructionType inst, size_t size) {
    return EncodeInstruction(inst, size, 0);
  }

  // Calculates the number of bytes needed to store the given size value as a
  // variable-length integer (VarintBE).
  static size_t CalculateLengthOfSizeAsVarint(size_t size);

  // Appends the size value to the string as a variable-length integer.
  static void AppendSizeToString(size_t size, string* out);

  // Appends the size value to the output string as a variable-length integer.
  static void AppendSizeToOutputString(size_t size, OutputStringInterface* out);

  // Calculates the "Length of the delta encoding" field for the delta window
  // header, based on the sizes of the sections and of the other header
  // elements.
  size_t CalculateLengthOfTheDeltaEncoding() const;

  // None of the following 'string' objects are null-terminated.

  // A series of instruction opcodes, each of which may be followed
  // by one or two Varint values representing the size parameters
  // of the first and second instruction in the opcode.
  string instructions_and_sizes_;

  // A series of data arguments (byte values) used for ADD and RUN
  // instructions.  Depending on whether interleaved output is used
  // for streaming or not, the pointer may point to
  // separate_data_for_add_and_run_ or to instructions_and_sizes_.
  string *data_for_add_and_run_;
  string separate_data_for_add_and_run_;

  // A series of Varint addresses used for COPY instructions.
  // For the SAME mode, a byte value is stored instead of a Varint.
  // Depending on whether interleaved output is used
  // for streaming or not, the pointer may point to
  // separate_addresses_for_copy_ or to instructions_and_sizes_.
  string *addresses_for_copy_;
  string separate_addresses_for_copy_;

  VCDiffAddressCache address_cache_;

  size_t dictionary_size_;

  // The number of bytes of target data that has been encoded so far.
  // Each time Add(), Copy(), or Run() is called, this will be incremented.
  // The target length is used to compute HERE mode addresses
  // for COPY instructions, and is also written into the header
  // of the delta window when Output() is called.
  //
  size_t target_length_;

  const VCDiffCodeTableData* code_table_data_;

  // The instruction map facilitates finding an opcode quickly given an
  // instruction inst, size, and mode.  This is an alternate representation
  // of the same information that is found in code_table_data_.
  //
  const VCDiffInstructionMap* instruction_map_;

  // The zero-based index within instructions_and_sizes_ of the byte
  // that contains the last single-instruction opcode generated by
  // EncodeInstruction().  (See that function for exhaustive details.)
  // It is necessary to use an index rather than a pointer for this value
  // because instructions_and_sizes_ may be resized, which would invalidate
  // any pointers into its data buffer.  The value -1 is reserved to mean that
  // either no opcodes have been generated yet, or else the last opcode
  // generated was a double-instruction opcode.
  //
  int last_opcode_index_;

  // If true, an Adler32 checksum of the target window data will be written as
  // a variable-length integer, just after the size of the addresses section.
  //
  bool add_checksum_;

  // The checksum to be written to the current target window,
  // if add_checksum_ is true.
  // This will not be calculated based on the individual calls to Add(), Run(),
  // and Copy(), which would be unnecessarily expensive.  Instead, the code
  // that uses the VCDiffCodeTableWriter object is expected to calculate
  // the checksum all at once and to call AddChecksum() with that value.
  // Must be called sometime before calling Output(), though it can be called
  // either before or after the calls to Add(), Run(), and Copy().
  //
  VCDChecksum checksum_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffCodeTableWriter(const VCDiffCodeTableWriter&);  // NOLINT
  void operator=(const VCDiffCodeTableWriter&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_ENCODETABLE_H_
