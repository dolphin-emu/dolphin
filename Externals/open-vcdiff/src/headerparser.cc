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

#include "headerparser.h"

#include <limits>

#include "logging.h"
#include "varint_bigendian.h"

namespace {

bool SumWouldOverflow2(size_t a, size_t b) {
  return a > std::numeric_limits<size_t>::max() - b;
}

bool SumWouldOverflow4(size_t a, size_t b, size_t c, size_t d) {
  return SumWouldOverflow2(a, b) ||
         SumWouldOverflow2(a + b, c) ||
         SumWouldOverflow2(a + b + c, d);
}

}  // namespace

namespace open_vcdiff {

// *** Methods for ParseableChunk

void ParseableChunk::Advance(size_t number_of_bytes) {
  if (number_of_bytes > UnparsedSize()) {
    VCD_DFATAL << "Internal error: position advanced by " << number_of_bytes
               << " bytes, current unparsed size " << UnparsedSize()
               << VCD_ENDL;
    position_ = end_;
    return;
  }
  position_ += number_of_bytes;
}

void ParseableChunk::SetPosition(const char* position) {
  if (position < start_) {
    VCD_DFATAL << "Internal error: new data position " << position
               << " is beyond start of data " << start_ << VCD_ENDL;
    position_ = start_;
    return;
  }
  if (position > end_) {
    VCD_DFATAL << "Internal error: new data position " << position
               << " is beyond end of data " << end_ << VCD_ENDL;
    position_ = end_;
    return;
  }
  position_ = position;
}

void ParseableChunk::FinishExcept(size_t number_of_bytes) {
  if (number_of_bytes > UnparsedSize()) {
    VCD_DFATAL << "Internal error: specified number of remaining bytes "
               << number_of_bytes << " is greater than unparsed data size "
               << UnparsedSize() << VCD_ENDL;
    Finish();
    return;
  }
  position_ = end_ - number_of_bytes;
}

// *** Methods for VCDiffHeaderParser

VCDiffHeaderParser::VCDiffHeaderParser(const char* header_start,
                                       const char* data_end)
    : parseable_chunk_(header_start, data_end - header_start),
      return_code_(RESULT_SUCCESS),
      delta_encoding_length_(0),
      delta_encoding_start_(NULL) { }

bool VCDiffHeaderParser::ParseByte(unsigned char* value) {
  if (RESULT_SUCCESS != return_code_) {
    return false;
  }
  if (parseable_chunk_.Empty()) {
    return_code_ = RESULT_END_OF_DATA;
    return false;
  }
  *value = static_cast<unsigned char>(*parseable_chunk_.UnparsedData());
  parseable_chunk_.Advance(1);
  return true;
}

bool VCDiffHeaderParser::ParseInt32(const char* variable_description,
                                    int32_t* value) {
  if (RESULT_SUCCESS != return_code_) {
    return false;
  }
  int32_t parsed_value =
      VarintBE<int32_t>::Parse(parseable_chunk_.End(),
                               parseable_chunk_.UnparsedDataAddr());
  switch (parsed_value) {
    case RESULT_ERROR:
      VCD_ERROR << "Expected " << variable_description
                << "; found invalid variable-length integer" << VCD_ENDL;
      return_code_ = RESULT_ERROR;
      return false;
    case RESULT_END_OF_DATA:
      return_code_ = RESULT_END_OF_DATA;
      return false;
    default:
      *value = parsed_value;
      return true;
  }
}

// When an unsigned 32-bit integer is expected, parse a signed 64-bit value
// instead, then check the value limit.  The uint32_t type can't be parsed
// directly because two negative values are given special meanings (RESULT_ERROR
// and RESULT_END_OF_DATA) and could not be expressed in an unsigned format.
bool VCDiffHeaderParser::ParseUInt32(const char* variable_description,
                                     uint32_t* value) {
  if (RESULT_SUCCESS != return_code_) {
    return false;
  }
  int64_t parsed_value =
      VarintBE<int64_t>::Parse(parseable_chunk_.End(),
                               parseable_chunk_.UnparsedDataAddr());
  switch (parsed_value) {
    case RESULT_ERROR:
      VCD_ERROR << "Expected " << variable_description
                << "; found invalid variable-length integer" << VCD_ENDL;
      return_code_ = RESULT_ERROR;
      return false;
    case RESULT_END_OF_DATA:
      return_code_ = RESULT_END_OF_DATA;
      return false;
    default:
      if (parsed_value > 0xFFFFFFFF) {
        VCD_ERROR << "Value of " << variable_description << "(" << parsed_value
                  << ") is too large for unsigned 32-bit integer" << VCD_ENDL;
        return_code_ = RESULT_ERROR;
        return false;
      }
      *value = static_cast<uint32_t>(parsed_value);
      return true;
  }
}

// A VCDChecksum represents an unsigned 32-bit value returned by adler32(),
// but isn't a uint32_t.
bool VCDiffHeaderParser::ParseChecksum(const char* variable_description,
                                       VCDChecksum* value) {
  uint32_t parsed_value = 0;
  if (!ParseUInt32(variable_description, &parsed_value)) {
    return false;
  }
  *value = static_cast<VCDChecksum>(parsed_value);
  return true;
}

bool VCDiffHeaderParser::ParseSize(const char* variable_description,
                                   size_t* value) {
  int32_t parsed_value = 0;
  if (!ParseInt32(variable_description, &parsed_value)) {
    return false;
  }
  *value = static_cast<size_t>(parsed_value);
  return true;
}

bool VCDiffHeaderParser::ParseSourceSegmentLengthAndPosition(
    size_t from_size,
    const char* from_boundary_name,
    const char* from_name,
    size_t* source_segment_length,
    size_t* source_segment_position) {
  // Verify the length and position values
  if (!ParseSize("source segment length", source_segment_length)) {
    return false;
  }
  // Guard against overflow by checking source length first
  if (*source_segment_length > from_size) {
    VCD_ERROR << "Source segment length (" << *source_segment_length
              << ") is larger than " << from_name << " (" << from_size
              << ")" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  if (!ParseSize("source segment position", source_segment_position)) {
    return false;
  }
  if ((*source_segment_position >= from_size) &&
      (*source_segment_length > 0)) {
    VCD_ERROR << "Source segment position (" << *source_segment_position
              << ") is past " << from_boundary_name
              << " (" << from_size << ")" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  const size_t source_segment_end = *source_segment_position +
                                    *source_segment_length;
  if (source_segment_end > from_size) {
    VCD_ERROR << "Source segment end position (" << source_segment_end
              << ") is past " << from_boundary_name
              << " (" << from_size << ")" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  return true;
}

bool VCDiffHeaderParser::ParseWinIndicatorAndSourceSegment(
    size_t dictionary_size,
    size_t decoded_target_size,
    bool allow_vcd_target,
    unsigned char* win_indicator,
    size_t* source_segment_length,
    size_t* source_segment_position) {
  if (!ParseByte(win_indicator)) {
    return false;
  }
  unsigned char source_target_flags =
      *win_indicator & (VCD_SOURCE | VCD_TARGET);
  switch (source_target_flags) {
    case VCD_SOURCE:
      return ParseSourceSegmentLengthAndPosition(dictionary_size,
                                                 "end of dictionary",
                                                 "dictionary",
                                                 source_segment_length,
                                                 source_segment_position);
    case VCD_TARGET:
      if (!allow_vcd_target) {
        VCD_ERROR << "Delta file contains VCD_TARGET flag, which is not "
                     "allowed by current decoder settings" << VCD_ENDL;
        return_code_ = RESULT_ERROR;
        return false;
      }
      return ParseSourceSegmentLengthAndPosition(decoded_target_size,
                                                 "current target position",
                                                 "target file",
                                                 source_segment_length,
                                                 source_segment_position);
    case VCD_SOURCE | VCD_TARGET:
      VCD_ERROR << "Win_Indicator must not have both VCD_SOURCE"
                   " and VCD_TARGET set" << VCD_ENDL;
      return_code_ = RESULT_ERROR;
      return false;
    default:
      return true;
  }
}

bool VCDiffHeaderParser::ParseWindowLengths(size_t* target_window_length) {
  if (delta_encoding_start_) {
    VCD_DFATAL << "Internal error: VCDiffHeaderParser::ParseWindowLengths "
                  "was called twice for the same delta window" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  if (!ParseSize("length of the delta encoding", &delta_encoding_length_)) {
    return false;
  }
  delta_encoding_start_ = UnparsedData();
  if (!ParseSize("size of the target window", target_window_length)) {
    return false;
  }
  return true;
}

const char* VCDiffHeaderParser::EndOfDeltaWindow() const {
  if (!delta_encoding_start_) {
    VCD_DFATAL << "Internal error: VCDiffHeaderParser::GetDeltaWindowEnd "
                  "was called before ParseWindowLengths" << VCD_ENDL;
    return NULL;
  }
  return delta_encoding_start_ + delta_encoding_length_;
}

bool VCDiffHeaderParser::ParseDeltaIndicator() {
  unsigned char delta_indicator;
  if (!ParseByte(&delta_indicator)) {
    return false;
  }
  if (delta_indicator & (VCD_DATACOMP | VCD_INSTCOMP | VCD_ADDRCOMP)) {
    VCD_ERROR << "Secondary compression of delta file sections "
                 "is not supported" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  return true;
}

bool VCDiffHeaderParser::ParseSectionLengths(
    bool has_checksum,
    size_t* add_and_run_data_length,
    size_t* instructions_and_sizes_length,
    size_t* addresses_length,
    VCDChecksum* checksum) {
  ParseSize("length of data for ADDs and RUNs", add_and_run_data_length);
  ParseSize("length of instructions section", instructions_and_sizes_length);
  ParseSize("length of addresses for COPYs", addresses_length);
  if (has_checksum) {
    ParseChecksum("Adler32 checksum value", checksum);
  }
  if (RESULT_SUCCESS != return_code_) {
    return false;
  }
  if (!delta_encoding_start_) {
    VCD_DFATAL << "Internal error: VCDiffHeaderParser::ParseSectionLengths "
                  "was called before ParseWindowLengths" << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  const size_t delta_encoding_header_length =
      UnparsedData() - delta_encoding_start_;
  if (SumWouldOverflow4(delta_encoding_header_length,
                        *add_and_run_data_length,
                        *instructions_and_sizes_length,
                        *addresses_length)) {
    VCD_ERROR << "The header + sizes of data sections would overflow the "
                 "maximum size"
              << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  if (delta_encoding_length_ !=
          (delta_encoding_header_length +
           *add_and_run_data_length +
           *instructions_and_sizes_length +
           *addresses_length)) {
    VCD_ERROR << "The length of the delta encoding does not match "
                 "the size of the header plus the sizes of the data sections"
              << VCD_ENDL;
    return_code_ = RESULT_ERROR;
    return false;
  }
  return true;
}

}  // namespace open_vcdiff
