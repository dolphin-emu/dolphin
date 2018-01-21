// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// HideObjectEngine
// Supports the removal of objects/effects from the rendering loop

#include "Common/StringUtil.h"
#include "Core/HideObjectEngine.h"
#include "Core/ConfigManager.h"
#include "Core/Core.h"

namespace HideObjectEngine
{
std::vector<std::string> HideObjectTypeStrings = {
    "8bits",  "16bits", "24bits", "32bits", "40bits",  "48bits",  "56bits",  "64bits",
    "72bits", "80bits", "88bits", "96bits", "104bits", "112bits", "120bits", "128bits"};

static std::vector<HideObject> HideObjectCodes;

void LoadHideObjectSection(const std::string& section, std::vector<HideObject>& HideObjectects,
                           IniFile& globalIni, IniFile& localIni)
{
  // Load the name of all enabled HideObjectects
  std::string enabledSectionName = section + "_Enabled";
  std::vector<std::string> enabledLines;
  std::set<std::string> enabledNames;
  localIni.GetLines(enabledSectionName, &enabledLines);
  for (const std::string& line : enabledLines)
  {
    if (line.size() != 0 && line[0] == '$')
    {
      std::string name = line.substr(1, line.size() - 1);
      enabledNames.insert(name);
    }
  }

  const IniFile* inis[2] = {&globalIni, &localIni};

  for (const IniFile* ini : inis)
  {
    std::vector<std::string> lines;
    HideObject currentHideObject;
    ini->GetLines(section, &lines);

    for (std::string& line : lines)
    {
      if (line.empty())
        continue;

      if (line[0] == '$')
      {
        // Take care of the previous code
        if (currentHideObject.name.size())
        {
          HideObjectects.push_back(currentHideObject);
        }
        currentHideObject.entries.clear();

        // Set active and name
        currentHideObject.name = line.substr(1, line.size() - 1);
        currentHideObject.active = enabledNames.find(currentHideObject.name) != enabledNames.end();
        currentHideObject.user_defined = (ini == &localIni);
      }
      else
      {
        std::string::size_type loc = line.find('=');

        if (loc != std::string::npos)
        {
          line[loc] = ':';
        }

        std::vector<std::string> items = SplitString(line, ':');

        if (items.size() >= 3)
        {
          HideObjectEntry parsedEntry;
          std::vector<std::string>::iterator it;
          bool success = true;

          it = std::find(HideObjectTypeStrings.begin(), HideObjectTypeStrings.end(), items[0]);
          parsedEntry.type = HideObjectType(it - HideObjectTypeStrings.begin());

          success &= (it != HideObjectTypeStrings.end());
          success &= TryParse(items[1], &parsedEntry.value_upper);
          success &= TryParse(items[2], &parsedEntry.value_lower);

          if (success)
          {
            currentHideObject.entries.push_back(parsedEntry);
          }
        }
      }
    }

    if (currentHideObject.name.size() && currentHideObject.entries.size())
    {
      HideObjectects.push_back(currentHideObject);
    }
  }
}

void LoadHideObjects()
{
  IniFile merged = SConfig::GetInstance().LoadGameIni();
  IniFile globalIni = SConfig::GetInstance().LoadDefaultGameIni();
  IniFile localIni = SConfig::GetInstance().LoadLocalGameIni();

  LoadHideObjectSection("HideObjectCodes", HideObjectCodes, globalIni, localIni);
}

void ApplyHideObjects(const std::vector<HideObject>& HideObjectects)
{
  // Make the rendering code skip over checking skip entries
  SConfig::GetInstance().hide_objects_updating = true;

  // Wait until the next time the next time the rendering thread finishes checking the skip entries.
  if (Core::IsRunning())
    while (SConfig::GetInstance().hide_objects_done == false)
    {
    }

  SConfig::GetInstance().object_removal_codes.clear();

  for (const HideObject& HideObjectect : HideObjectects)
  {
    if (HideObjectect.active)
    {
      for (const HideObjectEntry& entry : HideObjectect.entries)
      {
        u64 value_add_lower = entry.value_lower;
        u64 value_add_upper = entry.value_upper;
        SkipEntry skipEntry;
        int size = GetHideObjectTypeCharLength(entry.type) >> 1;

        if (size > 8)
        {
          size -= 8;
          for (int j = size; j > 0; --j)
          {
            skipEntry.push_back((0xFF & (value_add_upper >> ((j - 1) * 8))));
          }
          size = 8;
        }

        for (int j = size; j > 0; --j)
        {
          skipEntry.push_back((0xFF & (value_add_lower >> ((j - 1) * 8))));
        }

        SConfig::GetInstance().object_removal_codes.push_back(skipEntry);
      }
    }
  }
  SConfig::GetInstance().hide_objects_updating = false;
}

void ApplyFrameHideObjects()
{
  ApplyHideObjects(HideObjectCodes);
}

void Shutdown()
{
  HideObjectCodes.clear();
}

}  // namespace
