// SPDX-License-Identifier: CC0-1.0

#pragma once

// <strstream> is an often forgotten part of the C++ Standard Library. It was deprecated in C++98
// due to the hidden dangers in its design. However, unlike std::(o)strstream, std::istrstream is
// actually fairly safe, behaving similarly to C++23's std::ispanstream.

// MSVC emits a warning any time std::(io)strstream is used.
#ifdef _MSC_VER
#define _SILENCE_CXX17_STRSTREAM_DEPRECATION_WARNING
#if _HAS_CXX23 == true
#pragma message("It's time to replace <strstream> usage with <spanstream>")
#endif
#else
#include <version>
#ifdef __cpp_lib_spanstream
#warning "It's time to replace <strstream> usage with <spanstream>"
#endif
#endif

// GCC and Clang emit a warning from simply including the <strstream> header.
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-W#warnings"
#elif defined(__GNUC__)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-W#warnings"
#endif
#include <strstream>
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
