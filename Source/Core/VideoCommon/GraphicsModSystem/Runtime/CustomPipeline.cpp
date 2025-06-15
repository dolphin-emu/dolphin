// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "VideoCommon/GraphicsModSystem/Runtime/CustomPipeline.h"

#include <algorithm>
#include <array>
#include <variant>

#include "Common/Contains.h"
#include "Common/Logging/Log.h"
#include "Common/VariantUtil.h"

#include "VideoCommon/AbstractGfx.h"

namespace
{
bool IsQualifier(std::string_view value)
{
  static constexpr std::array<std::string_view, 7> qualifiers = {
      "attribute", "const", "highp", "lowp", "mediump", "uniform", "varying",
  };
  return Common::Contains(qualifiers, value);
}

bool IsBuiltInMacro(std::string_view value)
{
  static constexpr std::array<std::string_view, 5> built_in = {
      "__LINE__", "__FILE__", "__VERSION__", "GL_core_profile", "GL_compatibility_profile",
  };
  return Common::Contains(built_in, value);
}

std::vector<std::string> GlobalConflicts(std::string_view source)
{
  std::string_view last_identifier = "";
  std::vector<std::string> global_result;
  u32 scope = 0;
  for (u32 i = 0; i < source.size(); i++)
  {
    // If we're out of global scope, we don't care
    // about any of the details
    if (scope > 0)
    {
      if (source[i] == '{')
      {
        scope++;
      }
      else if (source[i] == '}')
      {
        scope--;
      }
      continue;
    }

    const auto parse_identifier = [&] {
      const u32 start = i;
      for (; i < source.size(); i++)
      {
        if (!Common::IsAlpha(source[i]) && source[i] != '_' && !std::isdigit(source[i]))
          break;
      }
      u32 end = i;
      i--;  // unwind
      return source.substr(start, end - start);
    };

    if (Common::IsAlpha(source[i]) || source[i] == '_')
    {
      const std::string_view identifier = parse_identifier();
      if (IsQualifier(identifier))
        continue;
      if (IsBuiltInMacro(identifier))
        continue;
      last_identifier = identifier;
    }
    else if (source[i] == '#')
    {
      const auto parse_until_end_of_preprocessor = [&] {
        bool continue_until_next_newline = false;
        for (; i < source.size(); i++)
        {
          if (source[i] == '\n')
          {
            if (continue_until_next_newline)
              continue_until_next_newline = false;
            else
              break;
          }
          else if (source[i] == '\\')
          {
            continue_until_next_newline = true;
          }
        }
      };
      i++;
      const std::string_view identifier = parse_identifier();
      if (identifier == "define")
      {
        i++;
        // skip whitespace
        while (source[i] == ' ')
        {
          i++;
        }
        global_result.emplace_back(parse_identifier());
        parse_until_end_of_preprocessor();
      }
      else
      {
        parse_until_end_of_preprocessor();
      }
    }
    else if (source[i] == '{')
    {
      scope++;
    }
    else if (source[i] == '(')
    {
      // Unlikely the user will be using layouts but...
      if (last_identifier == "layout")
        continue;

      // Since we handle equality, we can assume the identifier
      // before '(' is a function definition
      global_result.emplace_back(last_identifier);
    }
    else if (source[i] == '=')
    {
      global_result.emplace_back(last_identifier);
      i++;
      for (; i < source.size(); i++)
      {
        if (source[i] == ';')
          break;
      }
    }
    else if (source[i] == '/')
    {
      if ((i + 1) >= source.size())
        continue;

      if (source[i + 1] == '/')
      {
        // Go to end of line...
        for (; i < source.size(); i++)
        {
          if (source[i] == '\n')
            break;
        }
      }
      else if (source[i + 1] == '*')
      {
        // Multiline, look for first '*/'
        for (; i < source.size(); i++)
        {
          if (source[i] == '/' && source[i - 1] == '*')
            break;
        }
      }
    }
  }

  // Sort the conflicts from largest to smallest string
  // this way we can ensure smaller strings that are a substring
  // of the larger string are able to be replaced appropriately
  std::ranges::sort(global_result, std::ranges::greater{},
                    [](const std::string& s) { return s.size(); });
  return global_result;
}

}  // namespace

void CustomPipeline::UpdatePixelData(std::shared_ptr<VideoCommon::CustomAssetLibrary>,
                                     std::span<const u32>,
                                     const VideoCommon::CustomAssetLibrary::AssetID&)
{
}
