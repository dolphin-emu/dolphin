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

#ifndef OPEN_VCDIFF_VCDECODER_H_
#define OPEN_VCDIFF_VCDECODER_H_

#include <stddef.h>  // size_t
#include <string>
#include "output_string.h"

namespace open_vcdiff {

class VCDiffStreamingDecoderImpl;

// A streaming decoder class.  Takes a dictionary (source) file and a delta
// file, and produces the original target file.  It is intended to process
// the partial contents of the delta file as they arrive, in "chunks".
// As soon as a chunk of bytes is received from a file read or from a network
// transmission, it can be passed to DecodeChunk(), which will then output
// as much of the target file as it can.
//
// The client should use this class as follows:
//    VCDiffStreamingDecoder v;
//    v.StartDecoding(dictionary_ptr, dictionary_size);
//    while (any data left) {
//      if (!v.DecodeChunk(data, len, &output_string)) {
//        handle error;
//        break;
//      }
//      process(output_string);  // might have no new data, though
//    }
//    if (!v.FinishDecoding()) { ... handle error ... }
//
// I.e., the allowed pattern of calls is
//    StartDecoding DecodeChunk* FinishDecoding
//
// NOTE: It is not necessary to call FinishDecoding if DecodeChunk
//       returns false.  When DecodeChunk returns false to signal an
//       error, it resets its state and is ready for a new StartDecoding.
//       If FinishDecoding is called, it will also return false.
//
class VCDiffStreamingDecoder {
 public:
  VCDiffStreamingDecoder();
  ~VCDiffStreamingDecoder();

  // Resets the dictionary contents to "dictionary_ptr[0,dictionary_size-1]"
  // and sets up the data structures for decoding.  Note that the dictionary
  // contents are not copied, and the client is responsible for ensuring that
  // dictionary_ptr is valid until FinishDecoding is called.
  //
  void StartDecoding(const char* dictionary_ptr, size_t dictionary_size);

  // Accepts "data[0,len-1]" as additional data received in the
  // compressed stream.  If any chunks of data can be fully decoded,
  // they are appended to output_string.
  //
  // Returns true on success, and false if the data was malformed
  // or if there was an error in decoding it (e.g. out of memory, etc.).
  //
  // Note: we *append*, so the old contents of output_string stick around.
  // This convention differs from the non-streaming Encode/Decode
  // interfaces in VCDiffDecoder.
  //
  // output_string is guaranteed to be resized no more than once for each
  // window in the VCDIFF delta file.  This rule is irrespective
  // of the number of calls to DecodeChunk().
  //
  template<class OutputType>
  bool DecodeChunk(const char* data, size_t len, OutputType* output) {
    OutputString<OutputType> output_string(output);
    return DecodeChunkToInterface(data, len, &output_string);
  }

  bool DecodeChunkToInterface(const char* data, size_t len,
                              OutputStringInterface* output_string);

  // Finishes decoding after all data has been received.  Returns true
  // if decoding of the entire stream was successful.  FinishDecoding()
  // must be called for the current target before StartDecoding() can be
  // called for a different target.
  //
  bool FinishDecoding();

  // *** Adjustable parameters ***

  // Specifies the maximum allowable target file size.  If the decoder
  // encounters a delta file that would cause it to create a target file larger
  // than this limit, it will log an error and stop decoding.  If the decoder is
  // applied to delta files whose sizes vary greatly and whose contents can be
  // trusted, then a value larger than the the default value (64 MB) can be
  // specified to allow for maximum flexibility.  On the other hand, if the
  // input data is known never to exceed a particular size, and/or the input
  // data may be maliciously constructed, a lower value can be supplied in order
  // to guard against running out of memory or swapping to disk while decoding
  // an extremely large target file.  The argument must be between 0 and
  // INT32_MAX (2G); if it is within these bounds, the function will set the
  // limit and return true.  Otherwise, the function will return false and will
  // not change the limit.  Setting the limit to 0 will cause all decode
  // operations of non-empty target files to fail.
  bool SetMaximumTargetFileSize(size_t new_maximum_target_file_size);

  // Specifies the maximum allowable target *window* size.  (A target file is
  // composed of zero or more target windows.)  If the decoder encounters a
  // delta window that would cause it to create a target window larger
  // than this limit, it will log an error and stop decoding.
  bool SetMaximumTargetWindowSize(size_t new_maximum_target_window_size);

  // This interface must be called before StartDecoding().  If its argument
  // is true, then the VCD_TARGET flag can be specified to allow the source
  // segment to be chosen from the previously-decoded target data.  (This is the
  // default behavior.)  If it is false, then specifying the VCD_TARGET flag is
  // considered an error, and the decoder does not need to keep in memory any
  // decoded target data prior to the current window.
  void SetAllowVcdTarget(bool allow_vcd_target);

 private:
  VCDiffStreamingDecoderImpl* const impl_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  explicit VCDiffStreamingDecoder(const VCDiffStreamingDecoder&);
  void operator=(const VCDiffStreamingDecoder&);
};

// A simpler (non-streaming) interface to the VCDIFF decoder that can be used
// if the entire delta file is available.
//
class VCDiffDecoder {
 public:
  typedef std::string string;

  VCDiffDecoder() { }
  ~VCDiffDecoder() { }

  /***** Simple interface *****/

  // Replaces old contents of "*target" with the result of decoding
  // the bytes found in "encoding."
  //
  // Returns true if "encoding" was a well-formed sequence of
  // instructions, and returns false if not.
  //
  template<class OutputType>
  bool Decode(const char* dictionary_ptr,
              size_t dictionary_size,
              const string& encoding,
              OutputType* target) {
    OutputString<OutputType> output_string(target);
    return DecodeToInterface(dictionary_ptr,
                             dictionary_size,
                             encoding,
                             &output_string);
  }

 private:
  bool DecodeToInterface(const char* dictionary_ptr,
                         size_t dictionary_size,
                         const string& encoding,
                         OutputStringInterface* target);

  VCDiffStreamingDecoder decoder_;

  // Make the copy constructor and assignment operator private
  // so that they don't inadvertently get used.
  explicit VCDiffDecoder(const VCDiffDecoder&);
  void operator=(const VCDiffDecoder&);
};

};  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_VCDECODER_H_
