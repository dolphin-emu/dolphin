// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

// This header contains type definitions that are shared between the Dolphin core and
// other parts of the code. Any definitions that are only used by the core should be
// placed in "Common.h" instead.

#pragma once

#include <tchar.h>

#define __STRSAFE__NO_INLINE
#define STRSAFE_LIB
#ifndef _MSC_EXTENSIONS
#define _MSC_EXTENSIONS 1
#endif
#define NOCLIPBOARD

#include <strsafe.h>

#ifndef __is_convertible_to
#define __is_convertible_to(from, to) std::is_convertible<from, to>::value
#endif
#ifndef __is_abstract
#define __is_abstract(type) __is_abstract(type)
#endif
