// Copyright 2007 The open-vcdiff Authors. All Rights Reserved.
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

#ifndef OPEN_VCDIFF_VCENCODER_H_
#define OPEN_VCDIFF_VCENCODER_H_

#include <stddef.h>  // size_t
#include "format_extension_flags.h"
#include "output_string.h"

namespace open_vcdiff {

class VCDiffEngine;
class VCDiffStreamingEncoderImpl;
class CodeTableWriterInterface;

// A HashedDictionary must be constructed from the dictionary data
// in order to use VCDiffStreamingEncoder.  If the same dictionary will
// be used to perform several encoding operations, then the caller should
// create the HashedDictionary once and cache it for reuse.  This object
// is thread-safe: the same const HashedDictionary can be used
// by several threads simultaneously, each with its own VCDiffStreamingEncoder.
//
// dictionary_contents is copied into the HashedDictionary, so the
// caller may free that string, if desired, after the constructor returns.
//
class HashedDictionary {
 public:
  HashedDictionary(const char* dictionary_contents,
                   size_t dictionary_size);
  ~HashedDictionary();

  // Init() must be called before using the HashedDictionary as an argument
  // to the VCDiffStreamingEncoder, or for any other purpose except
  // destruction.  It returns true if initialization succeeded, or false
  // if an error occurred, in which case the caller should destroy the object
  // without using it.
  bool Init();

  const VCDiffEngine* engine() const { return engine_; }

 private:
  const VCDiffEngine* engine_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  HashedDictionary(const HashedDictionary&);  // NOLINT
  void operator=(const HashedDictionary&);
};

// The standard streaming interface to the VCDIFF (RFC 3284) encoder.
// "Streaming" in this context means that, even though the entire set of
// input data to be encoded may not be available at once, the encoder
// can produce partial output based on what is available.  Of course,
// the caller should try to maximize the sizes of the data chunks passed
// to the encoder.
class VCDiffStreamingEncoder {
 public:
  // The HashedDictionary object passed to the constructor must remain valid,
  // without being deleted, for the lifetime of the VCDiffStreamingEncoder
  // object.
  //
  // format_extensions allows certain open-vcdiff extensions to the VCDIFF
  // format to be included in the encoded output.  These extensions are not
  // part of the RFC 3284 draft standard, so specifying any extension flags
  // will make the output compatible only with open-vcdiff, or with other
  // VCDIFF implementations that accept these extensions.  See above for an
  // explanation of each possible flag value.
  //
  // *** look_for_target_matches:
  // The VCDIFF format allows COPY instruction addresses to reference data from
  // the source (dictionary), or from previously encoded target data.
  //
  // If look_for_target_matches is false, then the encoder will only
  // produce COPY instructions that reference source data from the dictionary,
  // never from previously encoded target data.  This will speed up the encoding
  // process, but the encoded data will not be as compact.
  //
  // If this value is true, then the encoder will produce COPY instructions
  // that reference either source data or target data.  A COPY instruction from
  // the previously encoded target data may even extend into the range of the
  // data being produced by that same COPY instruction; for example, if the
  // previously encoded target data is "LA", then a single COPY instruction of
  // length 10 can produce the additional target data "LALALALALA".
  //
  // There is a third type of COPY instruction that starts within
  // the source data and extends from the end of the source data
  // into the beginning of the target data.  This VCDIFF encoder will never
  // produce a COPY instruction of this third type (regardless of the value of
  // look_for_target_matches) because the cost of checking for matches
  // across the source-target boundary would not justify its benefits.
  //
  // Second version of constructor uses provided CodeTableInterfaceWriter
  // pointer instead of constructing one based on format_extenstions and will
  // take ownership of it. It's useful when you want to gather some statistics
  // for ADD/COPY/RUN instructions and archived compression ratio.
  VCDiffStreamingEncoder(const HashedDictionary* dictionary,
                         VCDiffFormatExtensionFlags format_extensions,
                         bool look_for_target_matches);
  VCDiffStreamingEncoder(const HashedDictionary* dictionary,
                         VCDiffFormatExtensionFlags format_extensions,
                         bool look_for_target_matches,
                         CodeTableWriterInterface* writer);
  ~VCDiffStreamingEncoder();

  // The client should use these routines as follows:
  //    HashedDictionary hd(dictionary, dictionary_size);
  //    if (!hd.Init()) {
  //      HandleError();
  //      return;
  //    }
  //    string output_string;
  //    VCDiffStreamingEncoder v(hd, false, false);
  //    if (!v.StartEncoding(&output_string)) {
  //      HandleError();
  //      return;  // No need to call FinishEncoding()
  //    }
  //    Process(output_string.data(), output_string.size());
  //    output_string.clear();
  //    while (get data_buf) {
  //      if (!v.EncodeChunk(data_buf, data_len, &output_string)) {
  //        HandleError();
  //        return;  // No need to call FinishEncoding()
  //      }
  //      // The encoding is appended to output_string at each call,
  //      // so clear output_string once its contents have been processed.
  //      Process(output_string.data(), output_string.size());
  //      output_string.clear();
  //    }
  //    if (!v.FinishEncoding(&output_string)) {
  //      HandleError();
  //      return;
  //    }
  //    Process(output_string.data(), output_string.size());
  //    output_string.clear();
  //
  // I.e., the allowed pattern of calls is
  //    StartEncoding EncodeChunk* FinishEncoding
  //
  // The size of the encoded output depends on the sizes of the chunks
  // passed in (i.e. the chunking boundary affects compression).
  // However the decoded output is independent of chunk boundaries.

  // Sets up the data structures for encoding.
  // Writes a VCDIFF delta file header (as defined in RFC section 4.1)
  // to *output_string.
  //
  // Note: we *append*, so the old contents of *output_string stick around.
  // This convention differs from the non-streaming Encode/Decode
  // interfaces in VCDiffEncoder.
  //
  // If an error occurs, this function returns false; otherwise it returns true.
  // If this function returns false, the caller does not need to call
  // FinishEncoding or to do any cleanup except destroying the
  // VCDiffStreamingEncoder object.
  template<class OutputType>
  bool StartEncoding(OutputType* output) {
    OutputString<OutputType> output_string(output);
    return StartEncodingToInterface(&output_string);
  }

  bool StartEncodingToInterface(OutputStringInterface* output_string);

  // Appends compressed encoding for "data" (one complete VCDIFF delta window)
  // to *output_string.
  // If an error occurs (for example, if StartEncoding was not called
  // earlier or StartEncoding returned false), this function returns false;
  // otherwise it returns true.  The caller does not need to call FinishEncoding
  // or do any cleanup except destroying the VCDiffStreamingEncoder
  // if this function returns false.
  template<class OutputType>
  bool EncodeChunk(const char* data, size_t len, OutputType* output) {
    OutputString<OutputType> output_string(output);
    return EncodeChunkToInterface(data, len, &output_string);
  }

  bool EncodeChunkToInterface(const char* data, size_t len,
                              OutputStringInterface* output_string);

  // Finishes encoding and appends any leftover encoded data to *output_string.
  // If an error occurs (for example, if StartEncoding was not called
  // earlier or StartEncoding returned false), this function returns false;
  // otherwise it returns true.  The caller does not need to
  // do any cleanup except destroying the VCDiffStreamingEncoder
  // if this function returns false.
  template<class OutputType>
  bool FinishEncoding(OutputType* output) {
    OutputString<OutputType> output_string(output);
    return FinishEncodingToInterface(&output_string);
  }

  bool FinishEncodingToInterface(OutputStringInterface* output_string);

 private:
  VCDiffStreamingEncoderImpl* const impl_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  VCDiffStreamingEncoder(const VCDiffStreamingEncoder&);  // NOLINT
  void operator=(const VCDiffStreamingEncoder&);
};

// A simpler (non-streaming) interface to the VCDIFF encoder that can be used
// if the entire target data string is available.
//
class VCDiffEncoder {
 public:
  VCDiffEncoder(const char* dictionary_contents, size_t dictionary_size)
      : dictionary_(dictionary_contents, dictionary_size),
        encoder_(NULL),
        flags_(VCD_STANDARD_FORMAT),
        look_for_target_matches_(true) { }

  ~VCDiffEncoder() {
    delete encoder_;
  }

  // By default, VCDiffEncoder uses standard VCDIFF format.  This function
  // can be used before calling Encode(), to specify that interleaved format
  // and/or checksum format should be used.
  void SetFormatFlags(VCDiffFormatExtensionFlags flags) { flags_ = flags; }

  // By default, VCDiffEncoder looks for matches in the dictionary and also in
  // the previously encoded target data.  This function can be used before
  // calling Encode(), to specify whether or not target matching should be
  // enabled.
  void SetTargetMatching(bool look_for_target_matches) {
    look_for_target_matches_ = look_for_target_matches;
  }

  // Replaces old contents of output_string with the encoded form of
  // target_data.
  template<class OutputType>
  bool Encode(const char* target_data,
              size_t target_len,
              OutputType* output) {
    OutputString<OutputType> output_string(output);
    return EncodeToInterface(target_data, target_len, &output_string);
  }

 private:
  bool EncodeToInterface(const char* target_data,
                         size_t target_len,
                         OutputStringInterface* output_string);

  HashedDictionary dictionary_;
  VCDiffStreamingEncoder* encoder_;
  VCDiffFormatExtensionFlags flags_;
  bool look_for_target_matches_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  VCDiffEncoder(const VCDiffEncoder&);  // NOLINT
  void operator=(const VCDiffEncoder&);
};

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VCENCODER_H_
