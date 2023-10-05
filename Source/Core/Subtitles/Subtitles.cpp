#include "Subtitles.h"
#include "TranslationEntry.h"
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

    auto tl = TranslationEntry(FileName.to_str(), Translation.to_str(),
                               Miliseconds.is<double>() ? Miliseconds.get<double>() :
                                                          OSD::Duration::SHORT,
                               Color.is<double>() ? Color.get<double>() : OSD::Color::CYAN,
                               Enabled.is<bool>() ? Enabled.get<bool>() : true,
                               AllowDuplicate.is<bool>() ? AllowDuplicate.get<bool>() : false,
                               Scale.is<double>() ? Scale.get<double>() : 1,
                               Offset.is<double>() ? Offset.get<double>() : 0,
                               OffsetEnd.is<double>() ? OffsetEnd.get<double>() : 0,
                               DisplayOnTop.is<bool>() ? DisplayOnTop.get<bool>() : false);

    if (tl.Enabled)
    {
      //TranslationEntryGroup group = Translations.find(tl.Filename)->second;
      //group.Add(tl);
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
  auto topstack =
      OSD::OSDMessageStack(0, 0, OSD::MessageStackDirection::Downward, true, false, TopOSDStackName);
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
    
 /* auto group = Translations["str/0000.thp"];

  INFO_LOG_FMT(SUBTITLES, "0 = {} {}", group.translationLines[0].Offset, group.translationLines[0].Text);
  INFO_LOG_FMT(SUBTITLES, "1 = {} {}", group.translationLines[1].Offset, group.translationLines[1].Text);
  INFO_LOG_FMT(SUBTITLES, "2 = {} {}", group.translationLines[2].Offset, group.translationLines[2].Text);
  INFO_LOG_FMT(SUBTITLES, "3 = {} {}", group.translationLines[3].Offset, group.translationLines[3].Text);

  auto tl = group.GetTLForRelativeOffset(3025152);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "3025152 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "1222025153 = is zero {}", tl == 0);
  }
  tl = group.GetTLForRelativeOffset(4025152);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "4025152 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "4025152 = is zero {}", tl == 0);
  }
  tl = group.GetTLForRelativeOffset(7025152);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "7025152 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "7025152 = is zero {}", tl == 0);
  }
  tl = group.GetTLForRelativeOffset(9025152);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "9025152 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "9025152 = is zero {}", tl == 0);
  }
  tl = group.GetTLForRelativeOffset(9025153);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "9025153 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "9025153 = is zero {}", tl == 0);
  }
  tl = group.GetTLForRelativeOffset(1222025153);
  if (tl != 0)
  {
    INFO_LOG_FMT(SUBTITLES, "1222025153 = {} {}", tl->Offset, tl->Text);
  }
  else
  {
    INFO_LOG_FMT(SUBTITLES, "1222025153 = is zero {}", tl == 0);
  }*/
}

void OnFileAccess(const DiscIO::Volume& volume, const DiscIO::Partition& partition, u64 offset)
{
  //return;
  //auto group = Translations["str/0000.thp"];
  ////INFO_LOG_FMT(SUBTITLES, "0 = {} {}", group.translationLines[0].Offset, group.translationLines[0].Text);
  ////INFO_LOG_FMT(SUBTITLES, "1 = {} {}", group.translationLines[1].Offset, group.translationLines[1].Text);
  ////INFO_LOG_FMT(SUBTITLES, "2 = {} {}", group.translationLines[2].Offset, group.translationLines[2].Text);
  ////INFO_LOG_FMT(SUBTITLES, "3 = {} {}", group.translationLines[3].Offset, group.translationLines[3].Text);

  //auto tl = group.GetTLForRelativeOffset(3025152);
  //INFO_LOG_FMT(SUBTITLES, "0 = {} {}", tl->Offset, tl->Text);

  //tl = group.GetTLForRelativeOffset(4025152);
  //INFO_LOG_FMT(SUBTITLES, "1 = {} {}", tl->Offset, tl->Text);

  //tl = group.GetTLForRelativeOffset(7025152);
  //INFO_LOG_FMT(SUBTITLES, "2 = {} {}", tl->Offset, tl->Text);

  //tl = group.GetTLForRelativeOffset(9025152);
  //INFO_LOG_FMT(SUBTITLES, "5 = {} {}", tl->Offset, tl->Text);

  //tl = group.GetTLForRelativeOffset(9025153);
  //INFO_LOG_FMT(SUBTITLES, "6 = {} {}", tl->Offset, tl->Text);

  //tl = group.GetTLForRelativeOffset(1222025153);
  //INFO_LOG_FMT(SUBTITLES, "7 = is zero {}", tl==0);

  //return;
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
  INFO_LOG_FMT(SUBTITLES, "File {} File Offset {} Offset {} Relative offset {}", path,
               file_info->GetOffset(),
               offset, relativeOffset);

  if (Translations.count(path) == 0)
    return;           

  auto tl = Translations[path].GetTLForRelativeOffset((u32)relativeOffset);   
  INFO_LOG_FMT(SUBTITLES, "{} Lines count {}", tl!=0, Translations[path].translationLines.size());                    

  
  if (!tl)
    return;


  //auto msg = fmt::format("offset {}", tl->Offset);
  //OSD::AddMessage(msg, 5000, OSD::Color::GREEN,
  //                BottomOSDStackName, true,
  //                1);

  OSD::AddMessage(tl->Text, tl->Miliseconds, tl->Color,
                  tl->DisplayOnTop ? TopOSDStackName : BottomOSDStackName, !tl->AllowDuplicate,
                  tl->Scale);
}
}  // namespace Subtitles
