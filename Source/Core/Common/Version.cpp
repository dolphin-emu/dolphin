// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/Version.h"

#include <string>

#include "Common/scmrev.h"

namespace Common
{
#define EMULATOR_NAME "Dolphin"

#ifdef _DEBUG
#define BUILD_TYPE_STR "Debug "
#elif defined DEBUGFAST
#define BUILD_TYPE_STR "DebugFast "
#else
#define BUILD_TYPE_STR ""
#endif

#ifndef IS_PLAYBACK
#define SLIPPI_REV_STR "4.0.0-mainline-beta.9"  // netplay version
#else
#define SLIPPI_REV_STR "3.2.0"  // playback version
#endif

const std::string& GetScmRevStr()
{
#ifndef IS_PLAYBACK
  static const std::string scm_rev_str = "Mainline - Slippi (" SLIPPI_REV_STR ")" BUILD_TYPE_STR;
#else
  static const std::string scm_rev_str =
      "Mainline - Slippi (" SLIPPI_REV_STR ") - Playback" BUILD_TYPE_STR;
#endif
  return scm_rev_str;
}

const std::string& GetSemVerStr()
{
  static const std::string sem_ver_str = SLIPPI_REV_STR;
  return sem_ver_str;
}

const std::string& GetScmRevGitStr()
{
  static const std::string scm_rev_git_str = SCM_REV_STR;
  return scm_rev_git_str;
}

const std::string& GetScmDescStr()
{
  static const std::string scm_desc_str = SCM_DESC_STR;
  return scm_desc_str;
}

const std::string& GetScmBranchStr()
{
  static const std::string scm_branch_str = SCM_BRANCH_STR;
  return scm_branch_str;
}

const std::string& GetUserAgentStr()
{
  static const std::string user_agent_str = EMULATOR_NAME "/" SCM_DESC_STR;
  return user_agent_str;
}

const std::string& GetScmDistributorStr()
{
  static const std::string scm_distributor_str = SCM_DISTRIBUTOR_STR;
  return scm_distributor_str;
}

const std::string& GetScmUpdateTrackStr()
{
  static const std::string scm_update_track_str = SCM_UPDATE_TRACK_STR;
  return scm_update_track_str;
}

const std::string& GetNetplayDolphinVer()
{
#ifdef _WIN32
  static const std::string netplay_dolphin_ver = "Slippi-" SCM_DESC_STR " Win";
#elif __APPLE__
  static const std::string netplay_dolphin_ver = "Slippi-" SCM_DESC_STR " Mac";
#else
  static const std::string netplay_dolphin_ver = "Slippi-" SCM_DESC_STR " Lin";
#endif
  return netplay_dolphin_ver;
}

int GetScmCommitsAheadMaster()
{
  // Note this macro can be empty if the master branch does not exist.
  return SCM_COMMITS_AHEAD_MASTER + 0;
}

}  // namespace Common
