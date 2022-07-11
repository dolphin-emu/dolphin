// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

// Terrible C++20 stdlib workaround for buildbots.  Remove this file ASAP!

#ifndef __cpp_concepts
#error "C++ Concepts are unavailable!  Update your compiler!"
#endif

#include <version>

#ifdef __cpp_lib_concepts
#include <concepts>
#else
#include <type_traits>
namespace std
{
template <class T>
concept integral = std::is_integral_v<T>;
};  // namespace std
#endif
