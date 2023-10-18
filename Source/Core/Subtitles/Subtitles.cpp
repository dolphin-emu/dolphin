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
#include "Common/Logging/LogManager.h"
#include "Core/ConfigManager.h"
#include "DiscIO/Filesystem.h"
#include "Subtitles/Helpers.h"
#include "Subtitles/SubtitleEntry.h"
#include "VideoCommon/OnScreenDisplay.h"

namespace Subtitles
{
bool g_messageStacksInitialized = false;
bool g_subtitlesInitialized = false;
std::map<std::string, SubtitleEntryGroup> Translations;

void DeserializeSubtitlesJson(std::string& filepath)
{
  OSDInfo(fmt::format("Reading translations from: {}", filepath));

  std::string json;
  File::ReadFileToString(filepath, json);

  if (json == "")
    return;

  picojson::value v;
  std::string err = picojson::parse(v, json);
  if (!err.empty())
  {
    Error(fmt::format("Subtitle JSON Error: {} in {}", err, filepath));
    return;
  }

  if (!v.is<picojson::array>())
  {
    Error(fmt::format("Subtitle JSON Error: Not an array in {}", filepath));
    return;
  }

  auto arr = v.get<picojson::array>();
  for (auto item : arr)
  {
    const auto FileName = item.get("FileName");
    const auto Translation = item.get("Translation");
    const auto Miliseconds = item.get("Miliseconds");
    const auto Color = item.get("Color");
    const auto Enabled = item.get("Enabled");
    const auto AllowDuplicate = item.get("AllowDuplicate");
    const auto Scale = item.get("Scale");
    const auto Offset = item.get("Offset");
    const auto OffsetEnd = item.get("OffsetEnd");
    const auto DisplayOnTop = item.get("DisplayOnTop");
    const auto Timestamp = item.get("Timestamp");

    // fitler out disabled entries, to lighten lookup load
    bool enabled = Enabled.is<bool>() ? Enabled.get<bool>() : true;
    if (!enabled)
      continue;

    // FileName and Translation are required fields
    if (!FileName.is<std::string>() || !Translation.is<std::string>())
      continue;

    const u32 color = TryParsecolor(Color, OSD::Color::CYAN);

    std::string filename = FileName.to_str();
    std::string translation = Translation.to_str();

    auto tl = SubtitleEntry(
        filename, translation,
        Miliseconds.is<double>() ? Miliseconds.get<double>() : OSD::Duration::SHORT, color, enabled,
        AllowDuplicate.is<bool>() ? AllowDuplicate.get<bool>() : false,
        Scale.is<double>() ? Scale.get<double>() : 1,
        Offset.is<double>() ? Offset.get<double>() : 0,
        OffsetEnd.is<double>() ? OffsetEnd.get<double>() : 0,
        DisplayOnTop.is<bool>() ? DisplayOnTop.get<bool>() : false,
        Timestamp.is<double>() ? Timestamp.get<double>() : 0);

    Translations[tl.Filename].Add(tl);
  }
}

void RecursivelyReadTranslationJsons(const File::FSTEntry& folder, const std::string& filter)
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
        DeserializeSubtitlesJson(filepath);
      }
    }
  }
}

void IniitalizeOSDMessageStacks()
{
  if (g_messageStacksInitialized)
    return;

  auto bottomstack = OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Upward, true, true,
                                          BottomOSDStackName);
  OSD::AddMessageStack(bottomstack);

  auto topstack = OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Downward, true, false,
                                       TopOSDStackName);
  OSD::AddMessageStack(topstack);

  g_messageStacksInitialized = true;
}

void LoadSubtitlesForGame(const std::string& gameId)
{
  g_subtitlesInitialized = false;
  Translations.clear();

  auto subtitleDir = File::GetUserPath(D_SUBTITLES_IDX) + gameId;

  OSDInfo(fmt::format("Loading subtitles for {} from {}", gameId, subtitleDir));

  auto fileEnumerator = File::ScanDirectoryTree(subtitleDir, true);
  RecursivelyReadTranslationJsons(fileEnumerator, SubtitleFileExtension);

  if (Translations.empty())
    return;

  // ensure stuff is sorted, you never know what mess people will make in text files :)
  std::for_each(Translations.begin(), Translations.end(),
                [](std::pair<const std::string, SubtitleEntryGroup>& t) { t.second.Preprocess(); });

  IniitalizeOSDMessageStacks();

  g_subtitlesInitialized = true;
  Info(fmt::format("Subtitles loaded for {}", gameId));
}

void Reload()
{
  LoadSubtitlesForGame(SConfig::GetInstance().GetGameID());
}

void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset)
{
  if (!g_subtitlesInitialized)
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

  auto tl = Translations[path].GetSubtitle((u32)relativeOffset);

  if (!tl)
    return;

  OSD::AddMessage(tl->Text, tl->Miliseconds, tl->Color,
                  tl->DisplayOnTop ? TopOSDStackName : BottomOSDStackName, !tl->AllowDuplicate,
                  tl->Scale);
}
}  // namespace Subtitles
