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

#ifndef OPEN_VCDIFF_HEADERPARSER_H_
#define OPEN_VCDIFF_HEADERPARSER_H_

#include "config.h"
#include <stddef.h>  // NULL
#include <stdint.h>  // int32_t, uint32_t
#include "checksum.h"  // VCDChecksum
#include "vcdiff_defs.h"  // VCDiffResult

namespace open_vcdiff {

// This class contains a contiguous memory buffer with start and end pointers,
// as well as a position pointer which shows how much of the buffer has been
// parsed and how much remains.
//
// Because no virtual destructor is defined for ParseableChunk, a pointer to
// a child class of ParseableChunk must be destroyed using its specific type,
// rather than as a ParseableChunk*.
class ParseableChunk {
 public:
  ParseableChunk(const char* data_start, size_t data_size) {
    SetDataBuffer(data_start, data_size);
  }

  const char* End() const { return end_; }

  // The number of bytes remaining to be parsed.  This is not necessarily the
  // same as the initial size of the buffer; it changes with each call to
  // Advance().
  size_t UnparsedSize() const {
    return end_ - position_;
  }

  // The number of bytes that have already been parsed.
  size_t ParsedSize() const {
    return position_ - start_;
  }

  bool Empty() const { return 0 == UnparsedSize(); }

  // The start of the data remaining to be parsed.
  const char* UnparsedData() const { return position_; }

  // Returns a pointer to the start of the data remaining to be parsed.
  const char** UnparsedDataAddr() { return &position_; }

  // Moves the parsing position forward by number_of_bytes.
  void Advance(size_t number_of_bytes);

  // Jumps the parsing position to a new location.
  void SetPosition(const char* position);

  // Jumps the parsing position to the end of the data chunk.
  void Finish() {
    position_ = end_;
  }

  // Jumps the parsing position so that there are now number_of_bytes
  // bytes left to parse.  This number should be smaller than the size of data
  // to be parsed before the function was called.
  void FinishExcept(size_t number_of_bytes);

  void SetDataBuffer(const char* data_start, size_t data_size) {
    start_ = data_start;
    end_ = data_start + data_size;
    position_ = start_;
  }

 private:
  const char* start_;
  const char* end_;

  // The current parsing position within the data chunk.
  // Must always respect start_ <= position_ <= end_.
  const char* position_;

  // Making these private avoids implicit copy constructor & assignment operator
  ParseableChunk(const ParseableChunk&);
  void operator=(const ParseableChunk&);
};

// Represents one of the three sections in the delta window, as described in
// RFC section 4.3:
//     * Data section for ADDs and RUNs
//     * Instructions and sizes section
//     * Addresses section for COPYs
// When using the interleaved format, data and addresses are pulled from the
// instructions and sizes section rather than being stored in separate sections.
// For that reason, this class allows one DeltaWindowSection to be based on
// another, such that the same position pointer is shared by both sections;
// i.e., UnparsedDataAddr() returns the same value for both objects.
// To achieve this end, one extra level of indirection (a pointer to a
// ParseableChunk object) is added.
class DeltaWindowSection {
 public:
  DeltaWindowSection() : parseable_chunk_(NULL), owned_(true) { }

  ~DeltaWindowSection() {
    FreeChunk();
  }

  void Init(const char* data_start, size_t data_size) {
    if (owned_ && parseable_chunk_) {
      // Reuse the already-allocated ParseableChunk object.
      parseable_chunk_->SetDataBuffer(data_start, data_size);
    } else {
      parseable_chunk_ = new ParseableChunk(data_start, data_size);
      owned_ = true;
    }
  }

  void Init(DeltaWindowSection* original) {
    FreeChunk();
    parseable_chunk_ = original->parseable_chunk_;
    owned_ = false;
  }

  void Invalidate() { FreeChunk(); }

  bool IsOwned() const { return owned_; }

  // The following functions just pass their arguments to the underlying
  // ParseableChunk object.

  const char* End() const {
    return parseable_chunk_->End();
  }

  size_t UnparsedSize() const {
    return parseable_chunk_->UnparsedSize();
  }

  size_t ParsedSize() const {
    return parseable_chunk_->ParsedSize();
  }

  bool Empty() const {
    return parseable_chunk_->Empty();
  }

  const char* UnparsedData() const {
    return parseable_chunk_->UnparsedData();
  }

  const char** UnparsedDataAddr() {
    return parseable_chunk_->UnparsedDataAddr();
  }

  void Advance(size_t number_of_bytes) {
    return parseable_chunk_->Advance(number_of_bytes);
  }
 private:
  void FreeChunk() {
    if (owned_) {
      delete parseable_chunk_;
    }
    parseable_chunk_ = NULL;
  }

  // Will be NULL until Init() has been called.  If owned_ is true, this will
  // point to a ParseableChunk object that has been allocated with "new" and
  // must be deleted by this DeltaWindowSection object.  If owned_ is false,
  // this points at the parseable_chunk_ owned by a different DeltaWindowSection
  // object.  In this case, it is important to free the DeltaWindowSection which
  // does not own the ParseableChunk before (or simultaneously to) freeing the
  // DeltaWindowSection that owns it, or else deleted memory may be accessed.
  ParseableChunk* parseable_chunk_;
  bool owned_;

  // Making these private avoids implicit copy constructor & assignment operator
  DeltaWindowSection(const DeltaWindowSection&);
  void operator=(const DeltaWindowSection&);
};

// Used to parse the bytes and Varints that make up the delta file header
// or delta window header.
class VCDiffHeaderParser {
 public:
  // header_start should be the start of the header to be parsed;
  // data_end is the position just after the last byte of available data
  // (which may extend far past the end of the header.)
  VCDiffHeaderParser(const char* header_start, const char* data_end);

  // One of these functions should be called for each element of the header.
  // variable_description is a description of the value that we are attempting
  // to parse, and will only be used to create descriptive error messages.
  // If the function returns true, then the element was parsed successfully
  // and its value has been placed in *value.  If the function returns false,
  // then *value is unchanged, and GetResult() can be called to return the
  // reason that the element could not be parsed, which will be either
  // RESULT_ERROR (an error occurred), or RESULT_END_OF_DATA (the limit data_end
  // was reached before the end of the element to be parsed.)  Once one of these
  // functions has returned false, further calls to any of the Parse...
  // functions will also return false without performing any additional actions.
  // Typical usage is as follows:
  //     int32_t segment_length = 0;
  //     if (!header_parser.ParseInt32("segment length", &segment_length)) {
  //       return header_parser.GetResult();
  //     }
  //
  // The following example takes advantage of the fact that calling a Parse...
  // function after an error or end-of-data condition is legal and does nothing.
  // It can thus parse more than one element in a row and check the status
  // afterwards.  If the first call to ParseInt32() fails, the second will have
  // no effect:
  //
  //     int32_t segment_length = 0, segment_position = 0;
  //     header_parser.ParseInt32("segment length", &segment_length));
  //     header_parser.ParseInt32("segment position", &segment_position));
  //     if (RESULT_SUCCESS != header_parser.GetResult()) {
  //       return header_parser.GetResult();
  //     }
  //
  bool ParseByte(unsigned char* value);
  bool ParseInt32(const char* variable_description, int32_t* value);
  bool ParseUInt32(const char* variable_description, uint32_t* value);
  bool ParseChecksum(const char* variable_description, VCDChecksum* value);
  bool ParseSize(const char* variable_description, size_t* value);

  // Parses the first three elements of the delta window header:
  //
  //     Win_Indicator                            - byte
  //     [Source segment size]                    - integer (VarintBE format)
  //     [Source segment position]                - integer (VarintBE format)
  //
  // Returns true if the values were parsed successfully and the values were
  // found to be acceptable.  Returns false otherwise, in which case
  // GetResult() can be called to return the reason that the two values
  // could not be validated.  This will be either RESULT_ERROR (an error
  // occurred and was logged), or RESULT_END_OF_DATA (the limit data_end was
  // reached before the end of the values to be parsed.)  If return value is
  // true, then *win_indicator, *source_segment_length, and
  // *source_segment_position are populated with the parsed values.  Otherwise,
  // the values of these output arguments are undefined.
  //
  // dictionary_size: The size of the dictionary (source) file.  Used to
  //     validate the limits of source_segment_length and
  //     source_segment_position if the source segment is taken from the
  //     dictionary (i.e., if the parsed *win_indicator equals VCD_SOURCE.)
  // decoded_target_size: The size of the target data that has been decoded
  //     so far, including all target windows.  Used to validate the limits of
  //     source_segment_length and source_segment_position if the source segment
  //     is taken from the target (i.e., if the parsed *win_indicator equals
  //     VCD_TARGET.)
  // allow_vcd_target: If this argument is false, and the parsed *win_indicator
  //     is VCD_TARGET, then an error is produced; if true, VCD_TARGET is
  //     allowed.
  // win_indicator (output): Points to a single unsigned char (not an array)
  //     that will receive the parsed value of Win_Indicator.
  // source_segment_length (output): The parsed length of the source segment.
  // source_segment_position (output): The parsed zero-based index in the
  //     source/target file from which the source segment is to be taken.
  bool ParseWinIndicatorAndSourceSegment(size_t dictionary_size,
                                         size_t decoded_target_size,
                                         bool allow_vcd_target,
                                         unsigned char* win_indicator,
                                         size_t* source_segment_length,
                                         size_t* source_segment_position);

  // Parses the following two elements of the delta window header:
  //
  //     Length of the delta encoding             - integer (VarintBE format)
  //     Size of the target window                - integer (VarintBE format)
  //
  // Return conditions and values are the same as for
  // ParseWinIndicatorAndSourceSegment(), above.
  //
  bool ParseWindowLengths(size_t* target_window_length);

  // May only be called after ParseWindowLengths() has returned RESULT_SUCCESS.
  // Returns a pointer to the end of the delta window (which might not point to
  // a valid memory location if there is insufficient input data.)
  //
  const char* EndOfDeltaWindow() const;

  // Parses the following element of the delta window header:
  //
  //     Delta_Indicator                          - byte
  //
  // Because none of the bits in Delta_Indicator are used by this implementation
  // of VCDIFF, this function does not have an output argument to return the
  // value of that field.  It may return RESULT_SUCCESS, RESULT_ERROR, or
  // RESULT_END_OF_DATA as with the other Parse...() functions.
  //
  bool ParseDeltaIndicator();

  // Parses the following 3 elements of the delta window header:
  //
  //     Length of data for ADDs and RUNs - integer (VarintBE format)
  //     Length of instructions and sizes - integer (VarintBE format)
  //     Length of addresses for COPYs    - integer (VarintBE format)
  //
  // If has_checksum is true, it also looks for the following element:
  //
  //     Adler32 checksum            - unsigned 32-bit integer (VarintBE format)
  //
  // Return conditions and values are the same as for
  // ParseWinIndicatorAndSourceSegment(), above.
  //
  bool ParseSectionLengths(bool has_checksum,
                           size_t* add_and_run_data_length,
                           size_t* instructions_and_sizes_length,
                           size_t* addresses_length,
                           VCDChecksum* checksum);

  // If one of the Parse... functions returned false, this function
  // can be used to find the result code (RESULT_ERROR or RESULT_END_OF_DATA)
  // describing the reason for the most recent parse failure.  If none of the
  // Parse... functions has returned false, returns RESULT_SUCCESS.
  VCDiffResult GetResult() const {
    return return_code_;
  }

  // The following functions just pass their arguments to the underlying
  // ParseableChunk object.

  const char* End() const {
    return parseable_chunk_.End();
  }

  size_t UnparsedSize() const {
    return parseable_chunk_.UnparsedSize();
  }

  size_t ParsedSize() const {
    return parseable_chunk_.ParsedSize();
  }

  const char* UnparsedData() const {
    return parseable_chunk_.UnparsedData();
  }

 private:
  // Parses two variable-length integers representing the source segment length
  // and source segment position (== offset.)  Checks whether the source segment
  // length and position would cause it to exceed the size of the source file or
  // target file.  Returns true if the values were parsed successfully and the
  // values were found to be acceptable.  Returns false otherwise, in which case
  // GetResult() can be called to return the reason that the two values could
  // not be validated, which will be either RESULT_ERROR (an error occurred and
  // was logged), or RESULT_END_OF_DATA (the limit data_end was reached before
  // the end of the integers to be parsed.)
  // from_size: The requested size of the source segment.
  // from_boundary_name: A NULL-terminated string naming the end of the
  //     source or target file, used in error messages.
  // from_name: A NULL-terminated string naming the source or target file,
  //     also used in error messages.
  // source_segment_length (output): The parsed length of the source segment.
  // source_segment_position (output): The parsed zero-based index in the
  //     source/target file from which the source segment is to be taken.
  //
  bool ParseSourceSegmentLengthAndPosition(size_t from_size,
                                           const char* from_boundary_name,
                                           const char* from_name,
                                           size_t* source_segment_length,
                                           size_t* source_segment_position);

  ParseableChunk parseable_chunk_;

  // Contains the result code of the last Parse...() operation that failed
  // (RESULT_ERROR or RESULT_END_OF_DATA).  If no Parse...() method has been
  // called, or if all calls to Parse...() were successful, then this contains
  // RESULT_SUCCESS.
  VCDiffResult return_code_;

  // Will be zero until ParseWindowLengths() has been called.  After
  // ParseWindowLengths() has been called successfully, this contains the
  // parsed length of the delta encoding.
  size_t delta_encoding_length_;

  // Will be NULL until ParseWindowLengths() has been called.  After
  // ParseWindowLengths() has been called successfully, this points to the
  // beginning of the section of the current window titled "The delta encoding"
  // in the RFC, i.e., to the position just after the length of the delta
  // encoding.
  const char* delta_encoding_start_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffHeaderParser(const VCDiffHeaderParser&);
  void operator=(const VCDiffHeaderParser&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_HEADERPARSER_H_
