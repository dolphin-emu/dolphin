// Copyright 2014 The open-vcdiff Authors. All Rights Reserved.
// Author: Mostyn Bramley-Moore
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

#ifndef OPEN_VCDIFF_UNIQUE_PTR_H_
#define OPEN_VCDIFF_UNIQUE_PTR_H_

// std::auto_ptr is deprecated in C++11, in favor of std::unique_ptr.
// Since C++11 is not widely available yet, the macro below is used to
// select the best available option.

#include <memory>

#if __cplusplus >= 201103L && !defined(OPEN_VCDIFF_USE_AUTO_PTR) // C++11
#define UNIQUE_PTR std::unique_ptr
#else
#define UNIQUE_PTR std::auto_ptr
#endif  // __cplusplus >= 201103L && !defined(OPEN_VCDIFF_USE_AUTO_PTR)

#endif  // OPEN_VCDIFF_UNIQUE_PTR_H_
