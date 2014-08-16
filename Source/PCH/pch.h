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
#include <float.h>
#include <fstream>
#include <functional>
#ifndef _WIN32
#include <getopt.h>
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
#include <thread>
#include <time.h>
#include <type_traits>
#ifndef _WIN32
#include <unistd.h>
#endif
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#ifdef _WIN32

#if _MSC_FULL_VER < 180030723
#error Please update your build environment to VS2013 with Update 3 or later!
#endif

// This numeral indicates the "minimum system required" to run the resulting
// program. Dolphin targets Vista+, so it should be 0x0600. However in practice,
// _WIN32_WINNT just removes up-level API declarations from headers. This is a
// problem for XAudio2 and XInput, where dolphin expects to compile against the
// Win8+ versions of their headers. So while we really need Vista+ level of
// support, we declare Win8+ here globally. If this becomes a problem, the
// higher declaration can be contained to just the XAudio2/XInput related code.
#define _WIN32_WINNT 0x0602

// Exclude rarely-used stuff from Windows headers
#define WIN32_LEAN_AND_MEAN
// Don't include windows min/max definitions. They would conflict with the STL.
#define NOMINMAX
#include <Windows.h>

#endif

#include "Common/Common.h"
#include "Common/Thread.h"
