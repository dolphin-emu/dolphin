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

#ifndef OPEN_VCDIFF_COMPILE_ASSERT_H_
#define OPEN_VCDIFF_COMPILE_ASSERT_H_

#include "config.h"

namespace open_vcdiff {

// The VCD_COMPILE_ASSERT macro can be used to verify that a compile-time
// expression is true. For example, you could use it to verify the
// size of a static array:
//
//   VCD_COMPILE_ASSERT(ARRAYSIZE(content_type_names) == CONTENT_NUM_TYPES,
//                      content_type_names_incorrect_size);
//
// or to make sure a struct is smaller than a certain size:
//
//   VCD_COMPILE_ASSERT(sizeof(foo) < 128, foo_too_large);
//
// For the second argument to VCD_COMPILE_ASSERT, the programmer should supply
// a variable name that meets C++ naming rules, but that provides
// a description of the compile-time rule that has been violated.
// (In the example above, the name used is "foo_too_large".)
// If the expression is false, most compilers will issue a warning/error
// containing the name of the variable.
// This refinement (adding a descriptive variable name argument)
// is what differentiates VCD_COMPILE_ASSERT from Boost static asserts.

template <bool>
struct CompileAssert {
};

}  // namespace open_vcdiff

#if __cplusplus >= 201103L
#define VCD_COMPILE_ASSERT(expr, msg) static_assert(expr, #msg)
#else
#define VCD_COMPILE_ASSERT(expr, msg) \
  typedef open_vcdiff::CompileAssert<static_cast<bool>(expr)> \
      msg[static_cast<bool>(expr) ? 1 : -1]
#endif  // __cplusplus >= 201103L

// Implementation details of VCD_COMPILE_ASSERT:
//
// - VCD_COMPILE_ASSERT works by defining an array type that has -1
//   elements (and thus is invalid) when the expression is false.
//
// - The simpler definition
//
//     #define VCD_COMPILE_ASSERT(expr, msg) typedef char msg[(expr) ? 1 : -1]
//
//   does not work, as gcc supports variable-length arrays whose sizes
//   are determined at run-time (this is gcc's extension and not part
//   of the C++ standard).  As a result, gcc fails to reject the
//   following code with the simple definition:
//
//     int foo;
//     VCD_COMPILE_ASSERT(foo, msg); // not supposed to compile as foo is
//                               // not a compile-time constant.
//
// - By using the type CompileAssert<(static_cast<bool>(expr))>, we ensure that
//   expr is a compile-time constant.  (Template arguments must be
//   determined at compile-time.)
//
// - The array size is (static_cast<bool>(expr) ? 1 : -1), instead of simply
//
//     ((expr) ? 1 : -1).
//
//   This is to avoid running into a bug in MS VC 7.1, which
//   causes ((0.0) ? 1 : -1) to incorrectly evaluate to 1.

#endif  // OPEN_VCDIFF_COMPILE_ASSERT_H_
