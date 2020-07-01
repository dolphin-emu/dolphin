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
// A command-line interface to the open-vcdiff library.

#include <config.h>
#include <assert.h>
#include <errno.h>
#ifdef WIN32
#include <fcntl.h>
#include <io.h>
#endif  // WIN32
#include <stdio.h>
#include <string.h>  // strerror
#include <iostream>
#include <string>
#include <vector>
#include "gflags/gflags.h"
#include "google/vcdecoder.h"
#include "google/vcencoder.h"
#include "google/jsonwriter.h"
#include "google/encodetable.h"
#include "unique_ptr.h" // auto_ptr, unique_ptr

#ifndef HAS_GLOBAL_STRING
using std::string;
#endif  // !HAS_GLOBAL_STRING
using google::GetCommandLineFlagInfoOrDie;
using google::ShowUsageWithFlagsRestrict;
using google::SetVersionString;

static const size_t kDefaultMaxTargetSize = 1 << 26;      // 64 MB

// Definitions of command-line flags
DEFINE_string(dictionary, "",
              "File containing dictionary data (required)");
DEFINE_string(target, "",
              "Target file (default is stdin for encode, stdout for decode");
DEFINE_string(delta, "",
              "Encoded delta file (default is stdout for encode, "
              "stdin for decode");
// --buffersize is the maximum allowable size of a target window.
// This value may be increased if there is sufficient memory available.
DEFINE_uint64(buffersize, 1 << 20,  // 1 MB
              "Buffer size for reading input file");
DEFINE_bool(allow_vcd_target, true,
            "If false, the decoder issues an error when the VCD_TARGET flag "
            "is encountered");
DEFINE_bool(checksum, false,
            "Include an Adler32 checksum of the target data when encoding");
DEFINE_bool(interleaved, false, "Use interleaved format");
DEFINE_bool(json, false, "Output diff in the JSON format when encoding");
DEFINE_bool(stats, false, "Report compression percentage");
DEFINE_bool(target_matches, false, "Find duplicate strings in target data"
                                   " as well as dictionary data");
DEFINE_uint64(max_target_file_size, kDefaultMaxTargetSize,
              "Maximum target file size allowed by decoder");
DEFINE_uint64(max_target_window_size, kDefaultMaxTargetSize,
              "Maximum target window size allowed by decoder");

static const char* const kUsageString =
    " {encode | delta | decode | patch }[ <options> ]\n"
    "encode or delta: create delta file from dictionary and target file\n"
    "decode or patch: reconstruct target file from dictionary and delta file";

namespace open_vcdiff {

class VCDiffFileBasedCoder {
 public:
  VCDiffFileBasedCoder();
  ~VCDiffFileBasedCoder();

  // Once the command-line arguments have been parsed, these functions
  // will use the supplied options to carry out a file-based encode
  // or decode operation.
  bool Encode();
  bool Decode();
  bool DecodeAndCompare();  // for "vcdiff test"; compare target with original

 private:
  // Determines the size of the file.  The given file must be an input file
  // opened for reading only, not an input stream such as stdin.  The function
  // returns true and populates file_size if successful; otherwise, it returns
  // false.
  static bool FileSize(FILE* file, size_t* file_size);

  // Opens a file for incremental reading.  file_name is the name of the file
  // to be opened.  file_type should be a descriptive name (like "target") for
  // use in log messages.  If successful, returns true, sets *file to a valid
  // input file and resizes *buffer to the smaller of the the file size and the
  // --buffersize option.  If the function fails, it outputs a log message and
  // returns false.
  bool OpenFileForReading(const string& file_name,
                          const char* file_type,
                          FILE** file,
                          std::vector<char>* buffer);

  // Opens the dictionary file and reads it into a newly allocated buffer.
  // If successful, returns true and populates dictionary_ with the dictionary
  // contents; otherwise, returns false.
  bool OpenDictionary();

  // Opens the input file (the delta or target file) for reading.
  // Allocates space for the input buffer.  If successful,
  // input_file_ will be valid and input_buffer_ will be allocated.
  bool OpenInputFile() {
    return OpenFileForReading(input_file_name_,
                              input_file_type_,
                              &input_file_,
                              &input_buffer_);
  }

  // Opens the output file (the target or delta file) for writing.
  // If successful, output_file_ will be valid.
  bool OpenOutputFile();

  // Opens the output file (the target file) for comparison against the decoded
  // output when using "vcdiff test".
  bool OpenOutputFileForCompare() {
    return OpenFileForReading(output_file_name_,
                              output_file_type_,
                              &output_file_,
                              &compare_buffer_);
  }

  // Reads as much input data as possible from the input file
  // into input_buffer_.  If successful, returns true and sets *bytes_read
  // to the number of bytes read into input_buffer_.  If an error occurs,
  // writes an error log message and returns false.
  bool ReadInput(size_t* bytes_read);

  // Writes the contents of output to output_file_.  If successful, returns
  // true.  If an error occurs, writes an error log message and returns false.
  bool WriteOutput(const string& output);

  // Reads a number of bytes from output_file_ equal to the size of output,
  // and compares to make sure they match the contents of output.  If the bytes
  // do not match, or if end of file is reached before the expected number of
  // bytes have been read, or a read error occurs, the function returns false;
  // otherwise, returns true.
  bool CompareOutput(const string& output);

  // Dictionary contents.  The entire dictionary file will be read into memory.
  std::vector<char> dictionary_;

  UNIQUE_PTR<open_vcdiff::HashedDictionary> hashed_dictionary_;

  // These should be set to either "delta" or "target".  They are only
  // used in log messages such as "Error opening delta file..."
  const char* input_file_type_;
  const char* output_file_type_;

  // The filenames used for input and output.  Will be empty if stdin
  // or stdout is being used.
  string input_file_name_;
  string output_file_name_;

  // stdio-style file handles for the input and output files and the dictionary.
  // When encoding, input_file_ is the target file and output_file_ is the delta
  // file; when decoding, the reverse is true.  The dictionary is always read
  // from a file rather than from standard input.
  FILE* input_file_;
  FILE* output_file_;

  // A memory buffer used to load the input file into memory.  If the input
  // comes from stdin because no input file was specified, then the size of
  // input_buffer_ will be the value specified by the --buffersize option.
  // If the input comes from a file, then the buffer will be allocated to match
  // the file size, if possible.  However, the buffer will not exceed
  // --buffersize bytes in length.
  std::vector<char> input_buffer_;

  // A memory buffer used to load the output file into memory for comparison
  // if "vcdiff test" is specified.
  std::vector<char> compare_buffer_;

  // Making these private avoids implicit copy constructor & assignment operator
  VCDiffFileBasedCoder(const VCDiffFileBasedCoder&);  // NOLINT
  void operator=(const VCDiffFileBasedCoder&);
};

inline VCDiffFileBasedCoder::VCDiffFileBasedCoder()
    : input_file_type_(""),
      output_file_type_(""),
      input_file_(NULL),
      output_file_(NULL) { }

VCDiffFileBasedCoder::~VCDiffFileBasedCoder() {
  if (input_file_ && (input_file_ != stdin)) {
    fclose(input_file_);
    input_file_ = NULL;
  }
  if (output_file_ && (output_file_ != stdout)) {
    fclose(output_file_);
    output_file_ = NULL;
  }
}

bool VCDiffFileBasedCoder::FileSize(FILE* file, size_t* file_size) {
  long initial_position = ftell(file);
  if (fseek(file, 0, SEEK_END) != 0) {
    return false;
  }
  *file_size = static_cast<size_t>(ftell(file));
  if (fseek(file, initial_position, SEEK_SET) != 0) {
    return false;
  }
  return true;
}

bool VCDiffFileBasedCoder::OpenDictionary() {
  assert(dictionary_.empty());
  assert(!FLAGS_dictionary.empty());
  FILE* dictionary_file = fopen(FLAGS_dictionary.c_str(), "rb");
  if (!dictionary_file) {
    std::cerr << "Error opening dictionary file '" << FLAGS_dictionary
              << "': " << strerror(errno) << std::endl;
    return false;
  }
  size_t dictionary_size = 0U;
  if (!FileSize(dictionary_file, &dictionary_size)) {
    std::cerr << "Error finding size of dictionary file '" << FLAGS_dictionary
              << "': " << strerror(errno) << std::endl;
    return false;
  }
  dictionary_.resize(dictionary_size);
  if (dictionary_size > 0) {
    if (fread(&dictionary_[0], 1, dictionary_size, dictionary_file)
            != dictionary_size) {
      std::cerr << "Unable to read dictionary file '" << FLAGS_dictionary
                << "': " << strerror(errno) << std::endl;
      fclose(dictionary_file);
      dictionary_.clear();
      return false;
    }
  }
  fclose(dictionary_file);
  return true;
}

bool VCDiffFileBasedCoder::OpenFileForReading(const string& file_name,
                                              const char* file_type,
                                              FILE** file,
                                              std::vector<char>* buffer) {
  assert(buffer->empty());
  size_t buffer_size = 0U;
  if (!*file && file_name.empty()) {
#ifdef WIN32
    _setmode(_fileno(stdin), _O_BINARY);
#endif
    *file = stdin;
    buffer_size = static_cast<size_t>(FLAGS_buffersize);
  } else {
    if (!*file) {
      *file = fopen(file_name.c_str(), "rb");
      if (!*file) {
        std::cerr << "Error opening " << file_type << " file '"
                  << file_name << "': " << strerror(errno) << std::endl;
        return false;
      }
    }
    size_t file_size = 0U;
    if (!FileSize(*file, &file_size)) {
      std::cerr << "Error finding size of " << file_type << " file '"
                << file_name << "': " << strerror(errno) << std::endl;
      return false;
    }
    buffer_size = static_cast<size_t>(FLAGS_buffersize);
    if (file_size < buffer_size) {
      // Allocate just enough memory to store the entire file
      buffer_size = file_size;
    }
  }
  buffer->resize(buffer_size);
  return true;
}

// Opens the output file for streamed read operations using the
// standard C I/O library, i.e., fopen(), fwrite(), fclose().
// No output buffer is allocated because the encoded/decoded output
// is constructed progressively using a std::string object
// whose buffer is resized as needed.
bool VCDiffFileBasedCoder::OpenOutputFile() {
  if (output_file_name_.empty()) {
#ifdef WIN32
    _setmode(_fileno(stdout), _O_BINARY);
#endif
    output_file_ = stdout;
  } else {
    output_file_ = fopen(output_file_name_.c_str(), "wb");
    if (!output_file_) {
      std::cerr << "Error opening " << output_file_type_ << " file '"
                << output_file_name_
                << "': " << strerror(errno) << std::endl;
      return false;
    }
  }
  return true;
}

bool VCDiffFileBasedCoder::ReadInput(size_t* bytes_read) {
  // Read from file or stdin
  *bytes_read = fread(&input_buffer_[0], 1, input_buffer_.size(), input_file_);
  if (ferror(input_file_)) {
    std::cerr << "Error reading from " << input_file_type_ << " file '"
              << input_file_name_
              << "': " << strerror(errno) << std::endl;
    return false;
  }
  return true;
}

bool VCDiffFileBasedCoder::WriteOutput(const string& output) {
  if (!output.empty()) {
    // Some new output has been generated and is ready to be written
    // to the output file or to stdout.
    fwrite(output.data(), 1, output.size(), output_file_);
    if (ferror(output_file_)) {
      std::cerr << "Error writing " << output.size() << " bytes to "
                << output_file_type_ << " file '" << output_file_name_
                << "': " << strerror(errno) << std::endl;
      return false;
    }
  }
  return true;
}

bool VCDiffFileBasedCoder::CompareOutput(const string& output) {
  if (!output.empty()) {
    size_t output_size = output.size();
    // Some new output has been generated and is ready to be compared against
    // the output file.
    if (output_size > compare_buffer_.size()) {
      compare_buffer_.resize(output_size);
    }
    size_t bytes_read = fread(&compare_buffer_[0],
                              1,
                              output_size,
                              output_file_);
    if (ferror(output_file_)) {
      std::cerr << "Error reading from " << output_file_type_ << " file '"
                << output_file_name_ << "': " << strerror(errno) << std::endl;
      return false;
    }
    if (bytes_read < output_size) {
      std::cerr << "Decoded target is longer than original target file"
                << std::endl;
      return false;
    }
    if (output.compare(0, output_size, &compare_buffer_[0], bytes_read) != 0) {
      std::cerr << "Original target file does not match decoded target"
                << std::endl;
      return false;
    }
  }
  return true;
}

bool VCDiffFileBasedCoder::Encode() {
  input_file_type_ = "target";
  input_file_name_ = FLAGS_target;
  output_file_type_ = "delta";
  output_file_name_ = FLAGS_delta;
  if (!OpenDictionary() || !OpenInputFile() || !OpenOutputFile()) {
    return false;
  }
  // Issue 6: Visual Studio STL produces a runtime exception
  // if &dictionary_[0] is attempted for an empty dictionary.
  if (dictionary_.empty()) {
    hashed_dictionary_.reset(new open_vcdiff::HashedDictionary("", 0));
  } else {
    hashed_dictionary_.reset(
        new open_vcdiff::HashedDictionary(&dictionary_[0],
                                          dictionary_.size()));
  }
  if (!hashed_dictionary_->Init()) {
    std::cerr << "Error initializing hashed dictionary" << std::endl;
    return false;
  }
  VCDiffFormatExtensionFlags format_flags = open_vcdiff::VCD_STANDARD_FORMAT;
  UNIQUE_PTR<CodeTableWriterInterface> writer;
  if (FLAGS_interleaved) {
    format_flags |= open_vcdiff::VCD_FORMAT_INTERLEAVED;
  }
  if (FLAGS_checksum) {
    format_flags |= open_vcdiff::VCD_FORMAT_CHECKSUM;
  }
  if (FLAGS_json) {
    format_flags |= open_vcdiff::VCD_FORMAT_JSON;
    writer.reset(new JSONCodeTableWriter);
  } else {
    writer.reset(new VCDiffCodeTableWriter(FLAGS_interleaved));
  }

  open_vcdiff::VCDiffStreamingEncoder encoder(hashed_dictionary_.get(),
                                              format_flags,
                                              FLAGS_target_matches,
                                              writer.release());
  string output;
  size_t input_size = 0;
  size_t output_size = 0;
  {
    if (!encoder.StartEncoding(&output)) {
      std::cerr << "Error during encoder initialization" << std::endl;
      return false;
    }
  }
  do {
    size_t bytes_read = 0;
    if (!WriteOutput(output) || !ReadInput(&bytes_read)) {
      return false;
    }
    output_size += output.size();
    output.clear();
    if (bytes_read > 0) {
      input_size += bytes_read;
      if (!encoder.EncodeChunk(&input_buffer_[0], bytes_read, &output)) {
        std::cerr << "Error trying to encode data chunk of length "
                  << bytes_read << std::endl;
        return false;
      }
    }
  } while (!feof(input_file_));
  encoder.FinishEncoding(&output);
  if (!WriteOutput(output)) {
    return false;
  }
  output_size += output.size();
  output.clear();
  if (FLAGS_stats && (input_size > 0)) {
    std::cerr << "Original size: " << input_size
              << "\tCompressed size: " << output_size << " ("
              << ((static_cast<double>(output_size) / input_size) * 100)
              << "% of original)" << std::endl;
  }
  return true;
}

bool VCDiffFileBasedCoder::Decode() {
  input_file_type_ = "delta";
  input_file_name_ = FLAGS_delta;
  output_file_type_ = "target";
  output_file_name_ = FLAGS_target;
  if (!OpenDictionary() || !OpenInputFile() || !OpenOutputFile()) {
    return false;
  }

  open_vcdiff::VCDiffStreamingDecoder decoder;
  decoder.SetMaximumTargetFileSize(
      static_cast<size_t>(FLAGS_max_target_file_size));
  decoder.SetMaximumTargetWindowSize(
      static_cast<size_t>(FLAGS_max_target_window_size));
  decoder.SetAllowVcdTarget(FLAGS_allow_vcd_target);
  string output;
  size_t input_size = 0;
  size_t output_size = 0;
  // Issue 6: Visual Studio STL produces a runtime exception
  // if &dictionary_[0] is attempted for an empty dictionary.
  if (dictionary_.empty()) {
    decoder.StartDecoding("", 0);
  } else {
    decoder.StartDecoding(&dictionary_[0], dictionary_.size());
  }

  do {
    size_t bytes_read = 0;
    if (!ReadInput(&bytes_read)) {
      return false;
    }
    if (bytes_read > 0) {
      input_size += bytes_read;
      if (!decoder.DecodeChunk(&input_buffer_[0], bytes_read, &output)) {
        std::cerr << "Error trying to decode data chunk of length "
                  << bytes_read << std::endl;
        return false;
      }
    }
    if (!WriteOutput(output)) {
      return false;
    }
    output_size += output.size();
    output.clear();
  } while (!feof(input_file_));
  if (!decoder.FinishDecoding()) {
    std::cerr << "Decode error; '" << FLAGS_delta
              << " may not be a valid VCDIFF delta file" << std::endl;
    return false;
  }
  if (!WriteOutput(output)) {
    return false;
  }
  output_size += output.size();
  output.clear();
  if (FLAGS_stats && (output_size > 0)) {
    std::cerr << "Decompressed size: " << output_size
              << "\tCompressed size: " << input_size << " ("
              << ((static_cast<double>(input_size) / output_size) * 100)
              << "% of original)" << std::endl;
  }
  return true;
}

bool VCDiffFileBasedCoder::DecodeAndCompare() {
  input_file_type_ = "delta";
  input_file_name_ = FLAGS_delta;
  output_file_type_ = "target";
  output_file_name_ = FLAGS_target;
  if (!OpenDictionary() || !OpenInputFile() || !OpenOutputFileForCompare()) {
    return false;
  }

  open_vcdiff::VCDiffStreamingDecoder decoder;
  decoder.SetMaximumTargetFileSize(
      static_cast<size_t>(FLAGS_max_target_file_size));
  decoder.SetMaximumTargetWindowSize(
      static_cast<size_t>(FLAGS_max_target_window_size));
  decoder.SetAllowVcdTarget(FLAGS_allow_vcd_target);
  string output;
  size_t input_size = 0;
  size_t output_size = 0;
  // Issue 6: Visual Studio STL produces a runtime exception
  // if &dictionary_[0] is attempted for an empty dictionary.
  if (dictionary_.empty()) {
    decoder.StartDecoding("", 0);
  } else {
    decoder.StartDecoding(&dictionary_[0], dictionary_.size());
  }

  do {
    size_t bytes_read = 0;
    if (!ReadInput(&bytes_read)) {
      return false;
    }
    if (bytes_read > 0) {
      input_size += bytes_read;
      if (!decoder.DecodeChunk(&input_buffer_[0], bytes_read, &output)) {
        std::cerr << "Error trying to decode data chunk of length "
                  << bytes_read << std::endl;
        return false;
      }
    }
    if (!CompareOutput(output)) {
      return false;
    }
    output_size += output.size();
    output.clear();
  } while (!feof(input_file_));
  if (!decoder.FinishDecoding()) {
    std::cerr << "Decode error; '" << FLAGS_delta
              << " may not be a valid VCDIFF delta file" << std::endl;
    return false;
  }
  if (!CompareOutput(output)) {
    return false;
  }
  output_size += output.size();
  output.clear();
  if (fgetc(output_file_) != EOF) {
    std::cerr << "Decoded target is shorter than original target file"
              << std::endl;
    return false;
  }
  if (ferror(output_file_)) {
    std::cerr << "Error reading end-of-file indicator from target file"
              << std::endl;
    return false;
  }
  if (FLAGS_stats && (output_size > 0)) {
    std::cerr << "Decompressed size: " << output_size
              << "\tCompressed size: " << input_size << " ("
              << ((static_cast<double>(input_size) / output_size) * 100)
              << "% of original)" << std::endl;
  }
  return true;
}

}  // namespace open_vcdiff

int main(int argc, char** argv) {
  const char* const command_name = argv[0];
  google::SetUsageMessage(kUsageString);
  google::SetVersionString(OPEN_VCDIFF_VERSION);
  google::ParseCommandLineFlags(&argc, &argv, true);
  if (argc != 2) {
    std::cerr << command_name << ": Must specify exactly one command option"
              << std::endl;
    ShowUsageWithFlagsRestrict(command_name, "vcdiff");
    return 1;
  }
  const char* const command_option = argv[1];
  if (FLAGS_dictionary.empty()) {
    std::cerr << command_name << " " << command_option
              << ": Must specify --dictionary <file-name>" << std::endl;
    ShowUsageWithFlagsRestrict(command_name, "vcdiff");
    return 1;
  }
  if (!GetCommandLineFlagInfoOrDie("buffersize").is_default &&
       (FLAGS_buffersize == 0)) {
    std::cerr << command_name << ": Option --buffersize cannot be 0"
              << std::endl;
    ShowUsageWithFlagsRestrict(command_name, "vcdiff");
    return 1;
  }
  if ((strcmp(command_option, "encode") == 0) ||
      (strcmp(command_option, "delta") == 0)) {
    open_vcdiff::VCDiffFileBasedCoder coder;
    if (!coder.Encode()) {
      return 1;
    }
    // The destructor for VCDiffFileBasedCoder will clean up the open files
    // and allocated memory.
  } else if ((strcmp(command_option, "decode") == 0) ||
             (strcmp(command_option, "patch") == 0)) {
    open_vcdiff::VCDiffFileBasedCoder coder;
    if (!coder.Decode()) {
      return 1;
    }
  } else if ((strcmp(command_option, "test") == 0)) {
    // "vcdiff test" does not appear in the usage string, but can be
    // used for debugging.  It encodes, then decodes, then compares the result
    // with the original target. It expects the same arguments as
    // "vcdiff encode", with the additional requirement that the --target
    // and --delta file arguments must be specified, rather than using stdin
    // or stdout.  It produces a delta file just as for "vcdiff encode".
    if (FLAGS_target.empty() || FLAGS_delta.empty()) {
      std::cerr << command_name
                << " test: Must specify both --target <file-name>"
                   " and --delta <file-name>" << std::endl;
      return 1;
    }
    const string original_target(FLAGS_target);
    // Put coder into a separate scope.
    {
      open_vcdiff::VCDiffFileBasedCoder coder;
      if (!coder.Encode()) {
        return 1;
      }
    }
    {
      open_vcdiff::VCDiffFileBasedCoder coder;
      if (!coder.DecodeAndCompare()) {
        return 1;
      }
    }
  } else {
    std::cerr << command_name << ": Unrecognized command option "
              << command_option << std::endl;
    ShowUsageWithFlagsRestrict(command_name, "vcdiff");
    return 1;
  }
  return 0;
}
