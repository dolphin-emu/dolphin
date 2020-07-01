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

#ifndef OPEN_VCDIFF_LOGGING_H_
#define OPEN_VCDIFF_LOGGING_H_

#include "config.h"
#include <stdlib.h>  // exit
#include <iostream>

namespace open_vcdiff {

extern bool g_fatal_error_occurred;

inline void CheckFatalError() {
  if (g_fatal_error_occurred) {
    std::cerr.flush();
    exit(1);
  }
}

}  // namespace open_vcdiff

#define VCD_WARNING std::cerr << "WARNING: "
#define VCD_ERROR std::cerr << "ERROR: "
#ifndef NDEBUG
#define VCD_DFATAL open_vcdiff::g_fatal_error_occurred = true; \
                   std::cerr << "FATAL: "
#else  // NDEBUG
#define VCD_DFATAL VCD_ERROR
#endif  // !NDEBUG

#define VCD_ENDL std::endl; \
                 open_vcdiff::CheckFatalError();

#endif  // OPEN_VCDIFF_LOGGING_H_
