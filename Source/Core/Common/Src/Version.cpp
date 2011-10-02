// Copyright (C) 2003 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

#include "Common.h"
#include "scmrev.h"

#ifdef _DEBUG
	#define BUILD_TYPE_STR "Debug "
#elif defined DEBUGFAST
	#define BUILD_TYPE_STR "DebugFast "
#else
	#define BUILD_TYPE_STR ""
#endif

const char *scm_rev_str = "Dolphin "
#if !SCM_IS_MASTER
	"[" SCM_BRANCH_STR "] "
#endif

#ifdef __INTEL_COMPILER
	BUILD_TYPE_STR SCM_DESC_STR "-ICC";
#else
	BUILD_TYPE_STR SCM_DESC_STR;
#endif

#ifdef _M_X64
#define NP_ARCH "x64"
#else
#define NP_ARCH "x86"
#endif

#ifdef _WIN32
const char *netplay_dolphin_ver = SCM_DESC_STR " W" NP_ARCH;
#elif __APPLE__
const char *netplay_dolphin_ver = SCM_DESC_STR " M" NP_ARCH;
#else
const char *netplay_dolphin_ver = SCM_DESC_STR " L" NP_ARCH;
#endif
