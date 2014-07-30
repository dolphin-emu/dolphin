// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "scmrev.h"
#include "Common/Common.h"

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

#ifdef _WIN32
const char *netplay_dolphin_ver = SCM_DESC_STR " Win";
#elif __APPLE__
const char *netplay_dolphin_ver = SCM_DESC_STR " Mac";
#else
const char *netplay_dolphin_ver = SCM_DESC_STR " Lin";
#endif

const char *scm_rev_git_str = SCM_REV_STR;

const char *scm_desc_str = SCM_DESC_STR;
const char *scm_branch_str = SCM_BRANCH_STR;
