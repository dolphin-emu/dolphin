// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "Core/GeckoCodeConfig.h"

#include <algorithm>
#include <sstream>
#include <string>
#include <vector>

#include "Common/HttpRequest.h"
#include "Common/IniFile.h"
#include "Common/Logging/Log.h"
#include "Common/StringUtil.h"
#include "Core/CheatCodes.h"

namespace Gecko
{
std::vector<GeckoCode> DownloadCodes(std::string gametdb_id, bool* succeeded)
{
  std::string endpoint{"https://www.geckocodes.org/txt.php?txt=" + gametdb_id};
  Common::HttpRequest http;

  // Circumvent high-tech DDOS protection
  http.SetCookies("challenge=BitMitigate.com;");

  // The server always redirects once to the same location.
  http.FollowRedirects(1);

  const Common::HttpRequest::Response response = http.Get(endpoint);
  *succeeded = response.has_value();
  if (!response)
    return {};

  // temp vector containing parsed codes
  std::vector<GeckoCode> gcodes;

  // parse the codes
  std::istringstream ss(std::string(response->begin(), response->end()));

  std::string line;

  // seek past the header, get to the first code
  std::getline(ss, line);
  std::getline(ss, line);
  std::getline(ss, line);

  int read_state = 0;
  GeckoCode gcode;

  while ((std::getline(ss, line).good()))
  {
    // Remove \r at the end of the line for files using windows line endings, std::getline only
    // removes \n
    line = StripSpaces(line);

    if (line.empty())
    {
      // add the code
      if (!gcode.codes.empty())
        gcodes.push_back(gcode);
      gcode = GeckoCode();
      read_state = 0;
      continue;
    }

    switch (read_state)
    {
    // read new code
    case 0:
    {
      std::istringstream ssline(line);
      // stop at [ character (beginning of contributor name)
      std::getline(ssline, gcode.name, '[');
      gcode.name = StripSpaces(gcode.name);
      gcode.user_defined = true;
      // read the code creator name
      std::getline(ssline, gcode.creator, ']');
      read_state = 1;
    }
    break;

    // read code lines
    case 1:
    {
      std::istringstream ssline(line);
      std::string addr, data;

      // Some locales (e.g. fr_FR.UTF-8) don't split the string stream on space
      // Use the C locale to workaround this behavior
      ssline.imbue(std::locale::classic());

      ssline >> addr >> data;
      ssline.seekg(0);

      // check if this line a code, silly, but the dumb txt file comment lines can start with
      // valid hex chars :/
      if (8 == addr.length() && 8 == data.length())
      {
        GeckoCode::Code new_code;
        new_code.original_line = line;
        ssline >> std::hex >> new_code.address >> new_code.data;
        gcode.codes.push_back(new_code);
      }
      else
      {
        gcode.notes.push_back(line);
        read_state = 2;  // start reading comments
      }
    }
    break;

    // read comment lines
    case 2:
      // append comment line
      gcode.notes.push_back(line);
      break;
    }
  }

  // add the last code
  if (!gcode.codes.empty())
    gcodes.push_back(gcode);

  return gcodes;
}

std::vector<GeckoCode> LoadCodes(const IniFile& globalIni, const IniFile& localIni)
{
  std::vector<GeckoCode> gcodes;

  for (const IniFile* ini : {&globalIni, &localIni})
  {
    std::vector<std::string> lines;
    ini->GetLines("Gecko", &lines, false);

    GeckoCode gcode;

    lines.erase(std::remove_if(lines.begin(), lines.end(),
                               [](const auto& line) { return line.empty() || line[0] == '#'; }),
                lines.end());

    for (auto& line : lines)
    {
      std::istringstream ss(line);

      // Some locales (e.g. fr_FR.UTF-8) don't split the string stream on space
      // Use the C locale to workaround this behavior
      ss.imbue(std::locale::classic());

      switch ((line)[0])
      {
      // enabled or disabled code
      case '+':
        ss.seekg(1);
        [[fallthrough]];
      case '$':
        if (!gcode.name.empty())
          gcodes.push_back(gcode);
        gcode = GeckoCode();
        gcode.enabled = (1 == ss.tellg());  // silly
        gcode.user_defined = (ini == &localIni);
        ss.seekg(1, std::ios_base::cur);
        // read the code name
        std::getline(ss, gcode.name, '[');  // stop at [ character (beginning of contributor name)
        gcode.name = StripSpaces(gcode.name);
        // read the code creator name
        std::getline(ss, gcode.creator, ']');
        break;

      // notes
      case '*':
        gcode.notes.push_back(std::string(++line.begin(), line.end()));
        break;

      // either part of the code, or an option choice
      default:
      {
        GeckoCode::Code new_code;
        // TODO: support options
        new_code.original_line = line;
        ss >> std::hex >> new_code.address >> new_code.data;
        gcode.codes.push_back(new_code);
      }
      break;
      }
    }

    // add the last code
    if (!gcode.name.empty())
    {
      gcodes.push_back(gcode);
    }

    ReadEnabledAndDisabled(*ini, "Gecko", &gcodes);

    if (ini == &globalIni)
    {
      for (GeckoCode& code : gcodes)
        code.default_enabled = code.enabled;
    }
  }

  return gcodes;
}

static std::string MakeGeckoCodeTitle(const GeckoCode& code)
{
  std::string title = '$' + code.name;

  if (!code.creator.empty())
  {
    title += " [" + code.creator + ']';
  }

  return title;
}

// used by the SaveGeckoCodes function
static void SaveGeckoCode(std::vector<std::string>& lines, const GeckoCode& gcode)
{
  if (!gcode.user_defined)
    return;

  lines.push_back(MakeGeckoCodeTitle(gcode));

  // save all the code lines
  for (const GeckoCode::Code& code : gcode.codes)
  {
    lines.push_back(code.original_line);
  }

  // save the notes
  for (const std::string& note : gcode.notes)
    lines.push_back('*' + note);
}

void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes)
{
  std::vector<std::string> lines;
  std::vector<std::string> enabled_lines;
  std::vector<std::string> disabled_lines;

  for (const GeckoCode& geckoCode : gcodes)
  {
    if (geckoCode.enabled)
      enabled_lines.emplace_back('$' + geckoCode.name);
    else if (geckoCode.default_enabled)
      disabled_lines.emplace_back('$' + geckoCode.name);

    SaveGeckoCode(lines, geckoCode);
  }

  inifile.SetLines("Gecko", lines);
  inifile.SetLines("Gecko_Enabled", enabled_lines);
  inifile.SetLines("Gecko_Disabled", disabled_lines);
}
}  // namespace Gecko
