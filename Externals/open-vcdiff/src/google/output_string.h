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

#ifndef OPEN_VCDIFF_OUTPUT_STRING_H_
#define OPEN_VCDIFF_OUTPUT_STRING_H_

#include <stddef.h>  // size_t

namespace open_vcdiff {

// This interface allows clients of VCDiff[Streaming]Encoder and
// VCDiff[Streaming]Decoder to use different string types to receive the output
// of those interfaces.
//
// Only the following operations can be performed on an output string, and their
// semantics must be identical to the std::string methods of the same names:
//     append()
//     clear()
//     push_back()
//     size()
//
// The versions of these methods that take a std::string argument are not
// supported by OutputStringInterface.
//
// There is one additional operation that can be performed on an output string:
// ReserveAdditionalBytes().  This asks the underlying output type to reserve
// enough capacity for the number of additional bytes requested in addition to
// existing content.  The decoder knows the total expected output size in
// advance, so one large ReserveAdditionalBytes() operation precedes many small
// append() operations.  For output types that gain no advantage from knowing in
// advance how many bytes will be appended, ReserveAdditionalBytes() can be
// defined to do nothing.
class OutputStringInterface {
 public:
  virtual ~OutputStringInterface() { }

  virtual OutputStringInterface& append(const char* s, size_t n) = 0;

  virtual void clear() = 0;

  virtual void push_back(char c) = 0;

  virtual void ReserveAdditionalBytes(size_t res_arg) = 0;

  virtual size_t size() const = 0;
};

// This template can be used to wrap any class that supports the operations
// needed by OutputStringInterface, including std::string.  A class that has
// different names or syntax for these operations will need specialized
// definitions of OutputString methods -- see output_string_types.h for some
// examples of how to do this.
template<class StringClass>
class OutputString : public OutputStringInterface {
 public:
  explicit OutputString(StringClass* impl) : impl_(impl) { }

  virtual ~OutputString() { }

  virtual OutputString& append(const char* s, size_t n) {
    impl_->append(s, n);
    return *this;
  }

  virtual void clear() {
    impl_->clear();
  }

  virtual void push_back(char c) {
    impl_->push_back(c);
  }

  virtual void ReserveAdditionalBytes(size_t res_arg) {
    impl_->reserve(impl_->size() + res_arg);
  }

  virtual size_t size() const {
    return impl_->size();
  }

 protected:
  StringClass* impl_;

 private:
  // Making these private avoids implicit copy constructor & assignment operator
  OutputString(const OutputString&);
  void operator=(const OutputString&);
};

// Don't allow the OutputString template to be based upon a pointer to
// OutputStringInterface.  Enforce this restriction by defining this class to
// lack any functions expected of an OutputString.
template<> class OutputString<OutputStringInterface> { };

}  // namespace open_vcdiff

#endif  // OPEN_VCDIFF_OUTPUT_STRING_H_
