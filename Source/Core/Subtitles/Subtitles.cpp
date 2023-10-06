#include "Subtitles.h"
#include "TranslationEntry.h"
#include "WebColors.h"
#include "picojson.h"

#include <VideoCommon/OnScreenDisplay.h>
#include "Core/ConfigManager.h"

#include <Common\FileUtil.h>
#include <DiscIO\Filesystem.h>

#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace Subtitles
{
bool _messageStacksInitialized = false;
bool _subtitlesInitialized = false;
std::map<std::string, TranslationEntryGroup> Translations;

void Info(std::string msg)
{
  OSD::AddMessage(msg, 2000, OSD::Color::GREEN);
  INFO_LOG_FMT(SUBTITLES, "{}", msg);
}
void Error(std::string err)
{
  OSD::AddMessage(err, 2000, OSD::Color::RED);
  ERROR_LOG_FMT(SUBTITLES, "{}", err);
}

void DeserializeSubtitlesJson(std::string json)
{
  if (json == "")
  {
    return;
  }

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

    if (!FileName.is<std::string>() || !Translation.is<std::string>())
    {
      continue;
    }

    u32 color = OSD::Color::CYAN;

    if (Color.is<double>())
      color = Color.get<double>();
    else
    {
      auto str = Color.to_str();
      Common::ToLower(&str);

      if (str.starts_with("0x"))
        //hex string
        color = std::stoul(str, nullptr, 16);
      else if (WebColors.count(str) == 1)
        //html color name
        color = WebColors[str];
      else
        //color noted with 3 or 4 base10 numers (rgb/argb)
        try //string parsing suucks
        {
          // try parse (a)rgb space delimited color
          u32 a, r, g, b;
          auto parts = SplitString(str, ' ');
          if (parts.size() == 4)
          {
            a = std::stoul(parts[0], nullptr, 10);
            r = std::stoul(parts[1], nullptr, 10);
            g = std::stoul(parts[2], nullptr, 10);
            b = std::stoul(parts[3], nullptr, 10);
            color = a << 24 | r << 16 | g << 8 | b;
          }
          else if (parts.size() == 3)
          {
            a = 255;
            r = std::stoul(parts[0], nullptr, 10);
            g = std::stoul(parts[1], nullptr, 10);
            b = std::stoul(parts[2], nullptr, 10);
            color = a << 24 | r << 16 | g << 8 | b;
          }
        }
        catch (std::exception x)
        {
          Error("Invalid color: " + str);
        }
    }

    auto tl = TranslationEntry(FileName.to_str(), Translation.to_str(),
                               Miliseconds.is<double>() ? Miliseconds.get<double>() :
                                                          OSD::Duration::SHORT,
                               color, Enabled.is<bool>() ? Enabled.get<bool>() : true,
                               AllowDuplicate.is<bool>() ? AllowDuplicate.get<bool>() : false,
                               Scale.is<double>() ? Scale.get<double>() : 1,
                               Offset.is<double>() ? Offset.get<double>() : 0,
                               OffsetEnd.is<double>() ? OffsetEnd.get<double>() : 0,
                               DisplayOnTop.is<bool>() ? DisplayOnTop.get<bool>() : false);

    if (tl.Enabled)
    {
      Translations[tl.Filename].Add(tl);
    }
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

  // ensure stuff is sorted, can't trust crap people will do in text files :)
  std::for_each(Translations.begin(), Translations.end(),
                [](std::pair<const std::string, TranslationEntryGroup>& t) { t.second.Sort(); });

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
