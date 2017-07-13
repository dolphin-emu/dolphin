// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#ifndef __has_cpp_attribute
#define __has_cpp_attribute(x) 0
#endif

#if __has_cpp_attribute(nodiscard)
#define NODISCARD [[nodiscard]]
#elif defined(__GNUC__) && (__GNUC__ >= 4)
#define NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER) && (_MSC_VER >= 1700)
// Only has an effect when code analysis is turned on
#define NODISCARD _Check_return_
#else
#define NODISCARD
#endif
