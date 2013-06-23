// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

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
#ifdef _M_ARM
#define NP_ARCH "ARM"
#else	
#define NP_ARCH "x86"
#endif
#endif

#ifdef _WIN32
const char *netplay_dolphin_ver = SCM_DESC_STR " W" NP_ARCH;
#elif __APPLE__
const char *netplay_dolphin_ver = SCM_DESC_STR " M" NP_ARCH;
#else
const char *netplay_dolphin_ver = SCM_DESC_STR " L" NP_ARCH;
#endif

const char *scm_rev_git_str = SCM_REV_STR;
