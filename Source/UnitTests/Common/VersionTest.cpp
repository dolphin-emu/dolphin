// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <iterator>
#include <regex>
#include <string_view>

#include <gtest/gtest.h>

#include "Common/Version.h"

static std::string_view GetStringViewFromIterators(const std::string_view::const_iterator begin,
                                                   const std::string_view::const_iterator end)
{
  const char* const begin_ptr = &(*begin);
  const int distance = std::distance(begin, end);
  // If begin follows end distance will be negative; ensure size is at least 0.
  const size_t view_size = static_cast<size_t>(std::max(0, distance));
  return std::string_view(begin_ptr, view_size);
}

static bool IsSCMDescStrValid(const std::string_view description)
{
  // If the build is not in a git repo the description string is simply "[Major].[Minor]", e.g. 5.0
  //
  // Otherwise it has the format "[Major].[Minor][-Patch][-dirty]", where -dirty is present if the
  // working tree is different from HEAD (i.e. you've made changes since the last commit), and Patch
  // is the number of commits between HEAD and the most recent tagged version. If HEAD is the most
  // recent tagged version -Patch is removed.

  // Example: "5.0-14021-dirty"
  static constexpr const char* desc_string_format =
      // Beginning of string
      "^"
      // Major.Minor
      "\\d+\\.\\d+"
      // Optional -[patch] (nonzero if present)
      "(?:-[1-9]\\d*)?"
      // Optional -dirty
      "(?:-dirty)?"
      // End of string
      "$";

  // Making the regexes in this file static reduces test running time by about 20%
  static const std::regex desc_string_regex(desc_string_format);
  const bool is_description_valid =
      std::regex_match(description.cbegin(), description.cend(), desc_string_regex);
  return is_description_valid;
}

static bool IsSCMRevStrValid(const std::string_view revision)
{
  // Example (brackets are part of string):
  // "Dolphin [branch-fix-all-the-things] Debug 5.0-14021-dirty"

  static constexpr const char* rev_string_format =
      // Beginning of string
      "^"
      "Dolphin "
      // Optional "[branch-name] "
      // Non-capturing group
      "(?:"
      // Left square bracket
      "\\["
      // Branch name
      ".+"
      // Right square bracket and space
      "\\] "
      // End of group, optional
      ")?"
      // "Debug ", "DebugFast ", or nothing
      "(?:Debug |DebugFast )?"
      // Capture scm_desc_str
      "(.+)"
      // Optional Intel Compiler flag
      "(?:-ICC)?"
      // End of string
      "$";

  static const std::regex rev_str_regex(rev_string_format);
  std::match_results<std::string_view::const_iterator> match;
  const bool main_match_result =
      std::regex_match(revision.cbegin(), revision.cend(), match, rev_str_regex);
  if (!main_match_result)
  {
    return false;
  }
  // Matches are pairs of iterators to beginning and end of captured groups.
  // match[0] is the whole string, match[1] is the first (and in this case only) captured group
  const std::string_view description = GetStringViewFromIterators(match[1].first, match[1].second);
  return IsSCMDescStrValid(description);
}

static bool IsSCMRevGitStrValid(const std::string_view hash)
{
  constexpr size_t git_hash_size = 40;
  const bool has_correct_size = hash.size() == git_hash_size;
  const auto is_lower_case_hex = [](const char c) { return std::isxdigit(c) && !std::isupper(c); };
  const bool is_lower_case_hex_string = std::all_of(hash.cbegin(), hash.cend(), is_lower_case_hex);
  return has_correct_size && is_lower_case_hex_string;
}

static bool IsSCMUpdateTrackStrValid(const std::string_view track)
{
  return track == "" || track == "stable" || track == "beta" || track == "dev";
}

static bool IsNetplayDolphinVerValid(const std::string_view version)
{
  constexpr size_t platform_size = 4;
  if (version.size() < platform_size)
  {
    return false;
  }
  const auto platform_start_iter = version.cend() - platform_size;
  const std::string_view description =
      GetStringViewFromIterators(version.cbegin(), platform_start_iter);
  const bool description_valid = IsSCMDescStrValid(description);
  const std::string_view platform = GetStringViewFromIterators(platform_start_iter, version.cend());
  const bool platform_valid = platform == " Win" || platform == " Mac" || platform == " Lin";
  return description_valid && platform_valid;
}

TEST(VersionTest, VersionHeaderValuesAreValid)
{
  // Skip testing Common::scm_branch_str and Common::scm_distributor_str because Dolphin doesn't
  // place any restrictions on their values

  EXPECT_TRUE(IsSCMDescStrValid(Common::scm_desc_str));
  EXPECT_TRUE(IsSCMRevStrValid(Common::scm_rev_str));
  EXPECT_TRUE(IsSCMRevGitStrValid(Common::scm_rev_git_str));
  EXPECT_TRUE(IsSCMUpdateTrackStrValid(Common::scm_update_track_str));
  EXPECT_TRUE(IsNetplayDolphinVerValid(Common::netplay_dolphin_ver));
}

TEST(VersionTest, ValidValuesPass)
{
  EXPECT_TRUE(IsSCMDescStrValid("5.0"));
  EXPECT_TRUE(IsSCMDescStrValid("5.0-dirty"));
  EXPECT_TRUE(IsSCMDescStrValid("5.0-1"));
  EXPECT_TRUE(IsSCMDescStrValid("5.0-14102-dirty"));
  EXPECT_TRUE(IsSCMDescStrValid("5.1"));
  EXPECT_TRUE(IsSCMDescStrValid("6.0"));
  EXPECT_TRUE(IsSCMDescStrValid("6.0-dirty"));
  EXPECT_TRUE(IsSCMDescStrValid("6.1-1-dirty"));

  EXPECT_TRUE(IsSCMRevStrValid("Dolphin 5.0"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin 5.0-dirty"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin 5.0-14021"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin [feature-branch] 5.0"));
  // ']' is legal character in branch name
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin []] 5.0"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin Debug 5.0"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin DebugFast 5.0"));
  EXPECT_TRUE(IsSCMRevStrValid("Dolphin [fix-all-the-things] Debug 5.0-14021-dirty"));

  EXPECT_TRUE(IsSCMRevGitStrValid("0000000000000000000000000000000000000000"));
  EXPECT_TRUE(IsSCMRevGitStrValid("0123456789abcdef0123456789abcdef01234567"));

  EXPECT_TRUE(IsSCMUpdateTrackStrValid(""));
  EXPECT_TRUE(IsSCMUpdateTrackStrValid("stable"));
  EXPECT_TRUE(IsSCMUpdateTrackStrValid("beta"));
  EXPECT_TRUE(IsSCMUpdateTrackStrValid("dev"));

  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0 Win"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0 Mac"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0 Lin"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0-dirty Win"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0-1 Mac"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.0-14102-dirty Lin"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("5.1 Win"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("6.0 Mac"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("6.0-dirty Lin"));
  EXPECT_TRUE(IsNetplayDolphinVerValid("6.1-1-dirty Win"));
}

TEST(VersionTest, InvalidValuesFail)
{
  EXPECT_FALSE(IsSCMDescStrValid("5."));
  EXPECT_FALSE(IsSCMDescStrValid(".0"));
  EXPECT_FALSE(IsSCMDescStrValid("50"));
  // Make sure '.' is escaped in IsSCMDescStrValid rather than acting as a wildcard character
  EXPECT_FALSE(IsSCMDescStrValid("5d0"));
  EXPECT_FALSE(IsSCMDescStrValid("5.0dirty"));
  EXPECT_FALSE(IsSCMDescStrValid("5.0 -dirty"));
  EXPECT_FALSE(IsSCMDescStrValid("5.0-dirty "));
  // If the patch number is 0 (i.e. this patch is the latest tagged commit), make sure it's removed
  EXPECT_FALSE(IsSCMDescStrValid("5.0-0"));
  // No leading zeroes for the patch number
  EXPECT_FALSE(IsSCMDescStrValid("5.0-014102"));
  // git describe adds the shortened commit hash to the description string, make sure it's removed
  EXPECT_FALSE(IsSCMDescStrValid("5.0-14102-g01234abcd"));

  // Check for incorrect spacing
  EXPECT_FALSE(IsSCMRevStrValid("Dolphin5.0"));
  EXPECT_FALSE(IsSCMRevStrValid("Dolphin  5.0"));
  EXPECT_FALSE(IsSCMRevStrValid("Dolphin [feature-branch]5.0"));
  EXPECT_FALSE(IsSCMRevStrValid("Dolphin Debug5.0"));
  EXPECT_FALSE(IsSCMRevStrValid("Dolphin DebugFast  5.0"));

  // Too long
  EXPECT_FALSE(IsSCMRevGitStrValid("00000000000000000000000000000000000000000"));
  // Too short
  EXPECT_FALSE(IsSCMRevGitStrValid("0123456789abcdef0123456789abcdef0123456"));
  // Upper case
  EXPECT_FALSE(IsSCMRevGitStrValid("0123456789ABCDEF0123456789ABCDEF01234567"));
  // Non-hex character
  EXPECT_FALSE(IsSCMRevGitStrValid("g000000000000000000000000000000000000000"));

  EXPECT_FALSE(IsSCMUpdateTrackStrValid(" "));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid("Stable"));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid(" beta"));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid("dev "));

  EXPECT_FALSE(IsNetplayDolphinVerValid("5.0Win"));
  EXPECT_FALSE(IsNetplayDolphinVerValid("5.0  Mac"));
  // Suffix should be " Lin"
  EXPECT_FALSE(IsNetplayDolphinVerValid("5.0 Linux"));
  EXPECT_FALSE(IsNetplayDolphinVerValid(" Win"));

  // Make sure swapping values causes a test failure

  EXPECT_FALSE(IsSCMDescStrValid(Common::scm_rev_str));
  EXPECT_FALSE(IsSCMDescStrValid(Common::scm_rev_git_str));
  EXPECT_FALSE(IsSCMDescStrValid(Common::scm_update_track_str));
  EXPECT_FALSE(IsSCMDescStrValid(Common::netplay_dolphin_ver));

  EXPECT_FALSE(IsSCMRevStrValid(Common::scm_desc_str));
  EXPECT_FALSE(IsSCMRevStrValid(Common::scm_rev_git_str));
  EXPECT_FALSE(IsSCMRevStrValid(Common::scm_update_track_str));
  EXPECT_FALSE(IsSCMRevStrValid(Common::netplay_dolphin_ver));

  EXPECT_FALSE(IsSCMRevGitStrValid(Common::scm_desc_str));
  EXPECT_FALSE(IsSCMRevGitStrValid(Common::scm_rev_str));
  EXPECT_FALSE(IsSCMRevGitStrValid(Common::scm_update_track_str));
  EXPECT_FALSE(IsSCMRevGitStrValid(Common::netplay_dolphin_ver));

  EXPECT_FALSE(IsSCMUpdateTrackStrValid(Common::scm_desc_str));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid(Common::scm_rev_str));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid(Common::scm_rev_git_str));
  EXPECT_FALSE(IsSCMUpdateTrackStrValid(Common::netplay_dolphin_ver));

  EXPECT_FALSE(IsNetplayDolphinVerValid(Common::scm_desc_str));
  EXPECT_FALSE(IsNetplayDolphinVerValid(Common::scm_rev_str));
  EXPECT_FALSE(IsNetplayDolphinVerValid(Common::scm_rev_git_str));
  EXPECT_FALSE(IsNetplayDolphinVerValid(Common::scm_update_track_str));
}
