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

SlippiDirectCodes::SlippiDirectCodes(std::string file_name)
{
  m_file_name = file_name;

  // Prevent additional file reads, if we've already loaded data to memory.
  // if (m_direct_code_infos.empty())
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
  std::string direct_codes_file_path = getCodesFilePath();

  INFO_LOG_FMT(SLIPPI_ONLINE, "Looking for direct codes file at {}", direct_codes_file_path);

  if (!File::Exists(direct_codes_file_path))
  {
    // Attempt to create empty file with array as parent json item.
    if (File::CreateFullPath(direct_codes_file_path) &&
        File::CreateEmptyFile(direct_codes_file_path))
    {
      File::WriteStringToFile(direct_codes_file_path, "[\n]");
    }
    else
    {
      WARN_LOG_FMT(SLIPPI_ONLINE, "Was unable to create {}", direct_codes_file_path.c_str());
    }
  }

  std::string direct_codes_file_contents;
  File::ReadFileToString(direct_codes_file_path, direct_codes_file_contents);

  m_direct_code_infos = parseFile(direct_codes_file_contents);
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
  for (auto it = m_direct_code_infos.begin(); it != m_direct_code_infos.end(); ++it)
  {
    if (it->connect_code == code)
    {
      found = true;
      it->last_played = timestamp;
    }
  }

  if (!found)
  {
    CodeInfo newDirectCode = {code, timestamp, false};
    m_direct_code_infos.push_back(newDirectCode);
  }

  // TODO: Maybe remove from here?
  // Or start a thread that is periodically called, if file writes will happen enough.
  WriteFile();
}

void SlippiDirectCodes::Sort(u8 sort_by_property)
{
  switch (sort_by_property)
  {
  case SORT_BY_TIME:
    std::sort(
        m_direct_code_infos.begin(), m_direct_code_infos.end(),
        [](const CodeInfo a, const CodeInfo b) -> bool { return a.last_played > b.last_played; });
    break;

  case SORT_BY_NAME:
    std::sort(
        m_direct_code_infos.begin(), m_direct_code_infos.end(),
        [](const CodeInfo a, const CodeInfo b) -> bool { return a.connect_code < b.connect_code; });
    break;
  }
}

std::string SlippiDirectCodes::Autocomplete(std::string start_text)
{
  // Pre-sort direct codes.
  Sort();

  // Find first entry in our sorted vector that starts with the given text.
  for (auto it = m_direct_code_infos.begin(); it != m_direct_code_infos.end(); it++)
  {
    if (it->connect_code.rfind(start_text, 0) == 0)
    {
      return it->connect_code;
    }
  }

  return start_text;
}

std::string SlippiDirectCodes::get(int index)
{
  Sort();

  if (index < m_direct_code_infos.size() && index >= 0)
  {
    return m_direct_code_infos.at(index).connect_code;
  }

  INFO_LOG_FMT(SLIPPI_ONLINE, "Out of bounds name entry index {}", index);

  return (index >= m_direct_code_infos.size()) ? "1" : "";
}

int SlippiDirectCodes::length()
{
  return (int)m_direct_code_infos.size();
}

void SlippiDirectCodes::WriteFile()
{
  std::string direct_codes_file_path = getCodesFilePath();

  // Outer empty array.
  json file_data = json::array();

  // Inner contents.
  json direct_code_data = json::object();

  // TODO Define constants for string literals.
  for (auto it = m_direct_code_infos.begin(); it != m_direct_code_infos.end(); ++it)
  {
    direct_code_data["connect_code"] = it->connect_code;
    direct_code_data["last_played"] = it->last_played;
    direct_code_data["is_favorite"] = it->is_favorite;

    file_data.emplace_back(direct_code_data);
  }

  File::WriteStringToFile(direct_codes_file_path, file_data.dump());
}

std::string SlippiDirectCodes::getCodesFilePath()
{
  std::string directCodesPath = File::GetUserPath(D_SLIPPI_IDX) + m_file_name;
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

std::vector<SlippiDirectCodes::CodeInfo> SlippiDirectCodes::parseFile(std::string file_contents)
{
  std::vector<SlippiDirectCodes::CodeInfo> direct_codes;

  json res = json::parse(file_contents, nullptr, false);
  // Unlike the user.json, the encapsulating type should be an array.
  if (res.is_discarded() || !res.is_array())
  {
    WARN_LOG_FMT(SLIPPI_ONLINE, "Malformed json in direct codes file.");
    return direct_codes;
  }

  // Retrieve all saved direct codes and related info
  for (auto it = res.begin(); it != res.end(); ++it)
  {
    if (it.value().is_object())
    {
      CodeInfo cur_direct_code;
      cur_direct_code.connect_code = readString(*it, "connect_code");
      cur_direct_code.last_played = readString(*it, "last_played");
      cur_direct_code.is_favorite = readBool(*it, "favorite");

      direct_codes.push_back(cur_direct_code);
    }
  }

  return direct_codes;
}
