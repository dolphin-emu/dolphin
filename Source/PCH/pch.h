// Copyright 2013 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#ifdef _WIN32

#define STRINGIFY_HELPER(x) #x
#define STRINGIFY(x) STRINGIFY_HELPER(x)

#if defined _MSC_FULL_VER && _MSC_FULL_VER < 193632532
#pragma message("Current _MSC_FULL_VER: " STRINGIFY(_MSC_FULL_VER))
#error Please update your build environment to the latest Visual Studio 2022!
#endif

#include <sdkddkver.h>
#ifndef NTDDI_WIN10_NI
#pragma message("Current WDK_NTDDI_VERSION: " STRINGIFY(WDK_NTDDI_VERSION))
#error Windows 10.0.22621 SDK or later is required
#endif

#undef STRINGIFY
#undef STRINGIFY_HELPER

#endif

#include <algorithm>
#include <array>
#include <assert.h>
#include <bitset>
#include <cassert>
#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <ctype.h>
#include <deque>
#include <errno.h>
#if !defined ANDROID && !defined _WIN32
#include <execinfo.h>
#endif
#include <fcntl.h>
#include <filesystem>
#include <float.h>
#include <fmt/format.h>
#include <fstream>
#include <functional>
#ifndef _WIN32
#include <getopt.h>
#endif
#if defined _WIN32 && defined _M_X86_64
#include <intrin.h>
#endif
#include <iomanip>
#include <iostream>
#include <limits.h>
#include <limits>
#include <list>
#include <locale.h>
#include <map>
#include <math.h>
#include <memory.h>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#ifndef _WIN32
#include <pthread.h>
#endif
#include <queue>
#include <set>
#include <sstream>
#include <stack>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <string_view>
#include <thread>
#include <time.h>
#include <type_traits>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>

#ifdef _WIN32
#include <Windows.h>
#endif

#include "Common/Common.h"
#include "Common/Thread.h"
