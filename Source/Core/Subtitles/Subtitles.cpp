// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Subtitles/Subtitles.h"

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

#include <picojson.h>

#include "Common/FileUtil.h"
#include "Core/ConfigManager.h"
#include "DiscIO/Filesystem.h"
#include "Subtitles/Helpers.h"
#include "Subtitles/SubtitleEntry.h"
#include "Subtitles/WebColors.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace Subtitles
{
bool _messageStacksInitialized = false;
bool _subtitlesInitialized = false;
std::map<std::string, SubtitleEntryGroup> Translations;

void DeserializeSubtitlesJson(std::string json)
{
  if (json == "")
    return;

  picojson::value v;
  std::string err = picojson::parse(v, json);
  if (!err.empty())
  {
    Error("Subtitle JSON Error: " + err);
    return;
  }

  if (!v.is<picojson::array>())
  {
    Error("Subtitle JSON Error: Not an array");
    return;
  }

  auto arr = v.get<picojson::array>();
  for (auto item : arr)
  {
    auto FileName = item.get("FileName");
    auto Translation = item.get("Translation");
    auto Miliseconds = item.get("Miliseconds");
    auto Color = item.get("Color");
    auto Enabled = item.get("Enabled");
    auto AllowDuplicate = item.get("AllowDuplicate");
    auto Scale = item.get("Scale");
    auto Offset = item.get("Offset");
    auto OffsetEnd = item.get("OffsetEnd");
    auto DisplayOnTop = item.get("DisplayOnTop");

    // FileName and Translation are required fields
    if (!FileName.is<std::string>() || !Translation.is<std::string>())
      continue;

    u32 color = TryParsecolor(Color, OSD::Color::CYAN);

    auto tl = SubtitleEntry(FileName.to_str(), Translation.to_str(),
                               Miliseconds.is<double>() ? Miliseconds.get<double>() :
                                                          OSD::Duration::SHORT,
                               color, Enabled.is<bool>() ? Enabled.get<bool>() : true,
                               AllowDuplicate.is<bool>() ? AllowDuplicate.get<bool>() : false,
                               Scale.is<double>() ? Scale.get<double>() : 1,
                               Offset.is<double>() ? Offset.get<double>() : 0,
                               OffsetEnd.is<double>() ? OffsetEnd.get<double>() : 0,
                               DisplayOnTop.is<bool>() ? DisplayOnTop.get<bool>() : false);

    //fitler out disabled entries, tp lighten lookup load
    if (tl.Enabled)
      Translations[tl.Filename].Add(tl);
  }
}

void RecursivelyReadTranslationJsons(const File::FSTEntry& folder, std::string filter)
{
  for (const auto& child : folder.children)
  {
    if (child.isDirectory)
    {
      RecursivelyReadTranslationJsons(child, filter);
    }
    else
    {
      auto filepath = child.physicalName;
      std::string extension;
      SplitPath(filepath, nullptr, nullptr, &extension);
      Common::ToLower(&extension);

      if (extension == filter)
      {
        Info("Reading translations from: " + filepath);

        std::string json;
        File::ReadFileToString(filepath, json);

        DeserializeSubtitlesJson(json);
      }
    }
  }
}

void IniitalizeOSDMessageStacks()
{
  if (_messageStacksInitialized)
    return;

  auto bottomstack = OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Upward, true, true,
                                          BottomOSDStackName);
  OSD::AddMessageStack(bottomstack);

  auto topstack = OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Downward, true, false,
                                       TopOSDStackName);
  OSD::AddMessageStack(topstack);

  _messageStacksInitialized = true;
}

void LoadSubtitlesForGame(std::string gameId)
{
  _subtitlesInitialized = false;
  Translations.clear();

  auto subtitleDir = File::GetUserPath(D_SUBTITLES_IDX) + gameId;

  auto fileEnumerator = File::ScanDirectoryTree(subtitleDir, true);
  RecursivelyReadTranslationJsons(fileEnumerator, ".json");

  if (Translations.empty())
    return;

  // ensure stuff is sorted, you never know what mess people will make in text files :)
  std::for_each(Translations.begin(), Translations.end(),
                [](std::pair<const std::string, SubtitleEntryGroup>& t) { t.second.Sort(); });

  IniitalizeOSDMessageStacks();

  _subtitlesInitialized = true;
}

void Reload()
{
  std::string game_id = SConfig::GetInstance().GetGameID();
  LoadSubtitlesForGame(game_id);
}

void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset)
{
  if (!_subtitlesInitialized)
    return;

  const DiscIO::FileSystem* file_system = volume.GetFileSystem(partition);
  if (!file_system)
    return;

  const std::unique_ptr<DiscIO::FileInfo> file_info = file_system->FindFileInfo(offset);

  if (!file_info)
    return;

  std::string path = file_info->GetPath();

  auto relativeOffset = offset - file_info->GetOffset();

  if (Translations.count(path) == 0)
    return;

  auto tl = Translations[path].GetTLForRelativeOffset((u32)relativeOffset);

  if (!tl)
    return;

  OSD::AddMessage(tl->Text, tl->Miliseconds, tl->Color,
                  tl->DisplayOnTop ? TopOSDStackName : BottomOSDStackName, !tl->AllowDuplicate,
                  tl->Scale);
}
}  // namespace Subtitles
