#include "SlippiDirectCodes.h"

#ifdef _WIN32
#include "AtlBase.h"
#include "AtlConv.h"
#endif

#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"
#include "Common/Thread.h"

#include "Core/ConfigManager.h"

#include <codecvt>
#include <locale>
#include <time.h>

#include <json.hpp>
using json = nlohmann::json;

SlippiDirectCodes::SlippiDirectCodes(std::string fileName)
{
  m_fileName = fileName;

  // Prevent additional file reads, if we've already loaded data to memory.
  // if (directCodeInfos.empty())
  ReadFile();
  Sort();
}

SlippiDirectCodes::~SlippiDirectCodes()
{
  // Add additional cleanup behavior here? Just added something
  // So compiler wouldn't nag.
  return;
}

void SlippiDirectCodes::ReadFile()
{
  std::string directCodesFilePath = getCodesFilePath();

  INFO_LOG_FMT(SLIPPI_ONLINE, "Looking for direct codes file at {}", directCodesFilePath.c_str());

  if (!File::Exists(directCodesFilePath))
  {
    // Attempt to create empty file with array as parent json item.
    if (File::CreateFullPath(directCodesFilePath) && File::CreateEmptyFile(directCodesFilePath))
    {
      File::WriteStringToFile("[\n]", directCodesFilePath);
    }
    else
    {
      WARN_LOG_FMT(SLIPPI_ONLINE, "Was unable to create {}", directCodesFilePath.c_str());
    }
  }

  std::string directCodesFileContents;
  File::ReadFileToString(directCodesFilePath, directCodesFileContents);

  directCodeInfos = parseFile(directCodesFileContents);
}

void SlippiDirectCodes::AddOrUpdateCode(std::string code)
{
  WARN_LOG_FMT(SLIPPI_ONLINE, "Attempting to add or update direct code: {}", code.c_str());

  time_t curTime;
  time(&curTime);
  u8 dateTimeStrLength = sizeof "20171015T095717";
  std::vector<char> dateTimeBuf(dateTimeStrLength);
  strftime(&dateTimeBuf[0], dateTimeStrLength, "%Y%m%dT%H%M%S", localtime(&curTime));
  std::string timestamp(&dateTimeBuf[0]);

  bool found = false;
  for (auto it = directCodeInfos.begin(); it != directCodeInfos.end(); ++it)
  {
    if (it->connectCode == code)
    {
      found = true;
      it->lastPlayed = timestamp;
    }
  }

  if (!found)
  {
    CodeInfo newDirectCode = {code, timestamp, false};
    directCodeInfos.push_back(newDirectCode);
  }

  // TODO: Maybe remove from here?
  // Or start a thread that is periodically called, if file writes will happen enough.
  WriteFile();
}

void SlippiDirectCodes::Sort(u8 sortByProperty)
{
  switch (sortByProperty)
  {
  case SORT_BY_TIME:
    std::sort(
        directCodeInfos.begin(), directCodeInfos.end(),
        [](const CodeInfo a, const CodeInfo b) -> bool { return a.lastPlayed > b.lastPlayed; });
    break;

  case SORT_BY_NAME:
    std::sort(
        directCodeInfos.begin(), directCodeInfos.end(),
        [](const CodeInfo a, const CodeInfo b) -> bool { return a.connectCode < b.connectCode; });
    break;
  }
}

std::string SlippiDirectCodes::Autocomplete(std::string startText)
{
  // Pre-sort direct codes.
  Sort();

  // Find first entry in our sorted vector that starts with the given text.
  for (auto it = directCodeInfos.begin(); it != directCodeInfos.end(); it++)
  {
    if (it->connectCode.rfind(startText, 0) == 0)
    {
      return it->connectCode;
    }
  }

  return startText;
}

std::string SlippiDirectCodes::get(int index)
{
  Sort();

  if (index < directCodeInfos.size() && index >= 0)
  {
    return directCodeInfos.at(index).connectCode;
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "Out of bounds name entry index {}", index);

  return (index >= directCodeInfos.size()) ? "1" : "";
}

int SlippiDirectCodes::length()
{
  return (int)directCodeInfos.size();
}

void SlippiDirectCodes::WriteFile()
{
  std::string directCodesFilePath = getCodesFilePath();

  // Outer empty array.
  json fileData = json::array();

  // Inner contents.
  json directCodeData = json::object();

  // TODO Define constants for string literals.
  for (auto it = directCodeInfos.begin(); it != directCodeInfos.end(); ++it)
  {
    directCodeData["connectCode"] = it->connectCode;
    directCodeData["lastPlayed"] = it->lastPlayed;
    directCodeData["isFavorite"] = it->isFavorite;

    fileData.emplace_back(directCodeData);
  }

  File::WriteStringToFile(fileData.dump(), directCodesFilePath);
}

std::string SlippiDirectCodes::getCodesFilePath()
{
  // TODO: Move to User dir
#if defined(__APPLE__)
  std::string directCodesPath =
      File::GetBundleDirectory() + "/Contents/Resources" + DIR_SEP + m_fileName;
#else
  std::string directCodesPath = File::GetUserPath(D_SLIPPI_IDX) + m_fileName;
#endif
  return directCodesPath;
}

inline std::string readString(json obj, std::string key)
{
  auto item = obj.find(key);
  if (item == obj.end() || item.value().is_null())
  {
    return "";
  }

  return obj[key];
}

inline bool readBool(json obj, std::string key)
{
  auto item = obj.find(key);
  if (item == obj.end() || item.value().is_null())
  {
    return false;
  }

  return obj[key];
}

std::vector<SlippiDirectCodes::CodeInfo> SlippiDirectCodes::parseFile(std::string fileContents)
{
  std::vector<SlippiDirectCodes::CodeInfo> directCodes;

  json res = json::parse(fileContents, nullptr, false);
  // Unlike the user.json, the encapsulating type should be an array.
  if (res.is_discarded() || !res.is_array())
  {
    WARN_LOG_FMT(SLIPPI_ONLINE, "Malformed json in direct codes file.");
    return directCodes;
  }

  // Retrieve all saved direct codes and related info
  for (auto it = res.begin(); it != res.end(); ++it)
  {
    if (it.value().is_object())
    {
      CodeInfo curDirectCode;
      curDirectCode.connectCode = readString(*it, "connectCode");
      curDirectCode.lastPlayed = readString(*it, "lastPlayed");
      curDirectCode.isFavorite = readBool(*it, "favorite");

      directCodes.push_back(curDirectCode);
    }
  }

  return directCodes;
}
