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
// Definition of an abstract class that describes the interface between the
// encoding engine (which finds the best string matches between the source and
// target data) and the code table writer.  The code table writer is passed a
// series of Add, Copy, and Run instructions and produces an output file in the
// desired format.

#ifndef OPEN_VCDIFF_CODETABLEWRITER_INTERFACE_H_
#define OPEN_VCDIFF_CODETABLEWRITER_INTERFACE_H_

#include <stddef.h>  // size_t
#include "../checksum.h";
#include "format_extension_flags.h"  // VCDiffFormatExtensionFlags

namespace open_vcdiff {

class OutputStringInterface;

// The method calls after construction should follow this pattern:
//    {{Add|Copy|Run}* Output}*
//
// Output() will produce an encoding using the given series of Add, Copy,
// and/or Run instructions.  One implementation of the interface
// (VCDiffCodeTableWriter) produces a VCDIFF delta window, but other
// implementations may be used to produce other output formats, or as test
// mocks, or to gather encoding statistics.
//
class CodeTableWriterInterface {
 public:
  virtual ~CodeTableWriterInterface() { }

  // Initializes the constructed object for use. It will return
  // false if there was an error initializing the object, or true if it
  // was successful.  After the object has been initialized and used,
  // Init() can be called again to restore the initial state of the object.
  virtual bool Init(size_t dictionary_size) = 0;

  // Writes the header to the output string.
  virtual void WriteHeader(OutputStringInterface* out,
                           VCDiffFormatExtensionFlags format_extensions) = 0;

  // Encode an ADD opcode with the "size" bytes starting at data
  virtual void Add(const char* data, size_t size) = 0;

  // Encode a COPY opcode with args "offset" (into dictionary) and "size" bytes.
  virtual void Copy(int32_t offset, size_t size) = 0;

  // Encode a RUN opcode for "size" copies of the value "byte".
  virtual void Run(size_t size, unsigned char byte) = 0;

  // Adds a checksum to the output.
  virtual void AddChecksum(VCDChecksum checksum) = 0;

  // Appends the encoded delta window to the output
  // string.  The output string is not null-terminated and may contain embedded
  // '\0' characters.
  virtual void Output(OutputStringInterface* out) = 0;

  // Finishes encoding.
  virtual void FinishEncoding(OutputStringInterface* out) = 0;

  // Verifies dictionary is compatible with writer.
  virtual bool VerifyDictionary(const char *dictionary, size_t size) const = 0;

  // Verifies target chunk is compatible with writer.
  virtual bool VerifyChunk(const char *chunk, size_t size) const = 0;
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_CODETABLEWRITER_INTERFACE_H_
