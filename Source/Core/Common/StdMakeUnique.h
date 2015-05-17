// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// VS 2013 defines __cplusplus as 199711L but has std::make_unique.
#if __cplusplus > 201103L || _MSC_VER >= 1800
#include <memory>
#else

// Implementation of C++14 std::make_unique, which is not supported by all the
// compilers we care about yet.
//
// NOTE: This code is based on the libc++ implementation of std::make_unique.
//
// Copyright (c) 2009-2014 by the libc++ contributors.
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to
// deal in the Software without restriction, including without limitation the
// rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
// sell copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
// FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
// IN THE SOFTWARE.

#include <cstddef>
#include <memory>
#include <type_traits>

namespace std
{

struct unspecified {};

template<class T>
struct unique_if
{
	typedef std::unique_ptr<T> unique_single;
};

template<class T>
struct unique_if<T[]>
{
	typedef std::unique_ptr<T[]> unique_array_unknown_bound;
};

template<class T, size_t N>
struct unique_if<T[N]>
{
	typedef void unique_array_known_bound;
};

template<class T, class... Args>
inline typename unique_if<T>::unique_single make_unique(Args&&... args)
{
	return std::unique_ptr<T>(new T(std::forward<Args>(args)...));
}

template<class T>
inline typename unique_if<T>::unique_array_unknown_bound make_unique(size_t n)
{
	typedef typename std::remove_extent<T>::type U;
	return std::unique_ptr<T>(new U[n]());
}

template<class T, class... Args>
typename unique_if<T>::unique_array_known_bound make_unique(Args&&...) = delete;

}  // namespace std

#endif // __cplusplus
