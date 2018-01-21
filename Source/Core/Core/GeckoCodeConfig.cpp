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

namespace Gecko
{
std::vector<GeckoCode> DownloadCodes(std::string gameid, bool* succeeded)
{
  switch (gameid[0])
  {
  case 'R':
  case 'S':
  case 'G':
    break;
  default:
    // All channels (WiiWare, VirtualConsole, etc) are identified by their first four characters
    gameid = gameid.substr(0, 4);
    break;
  }

  std::string endpoint{"http://geckocodes.org/txt.php?txt=" + gameid};
  Common::HttpRequest http;
  const Common::HttpRequest::Response response = http.Get(endpoint);
  *succeeded = response.has_value();
  if (!response)
    return {};

  // temp vector containing parsed codes
  std::vector<GeckoCode> gcodes;

  // parse the codes
  std::istringstream ss(reinterpret_cast<const char*>(response->data()));

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
      if (gcode.codes.size())
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
  if (gcode.codes.size())
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
      [](const std::string& line) { return line.empty() || line[0] == '#'; }),
                lines.end());

    for (auto& line : lines)
    {
      std::istringstream ss(line);

      switch ((line)[0])
      {
      // enabled or disabled code
      case '+':
        ss.seekg(1);
      case '$':
        if (gcode.name.size())
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
    if (gcode.name.size())
    {
      gcodes.push_back(gcode);
    }

    ini->GetLines("Gecko_Enabled", &lines, false);

    for (const std::string& line : lines)
    {
      if (line.empty() || line[0] != '$')
      {
        continue;
      }

      for (GeckoCode& ogcode : gcodes)
      {
        // Exclude the initial '$' from the comparison.
        if (line.compare(1, std::string::npos, ogcode.name) == 0)
        {
          ogcode.enabled = true;
        }
      }
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
static void SaveGeckoCode(std::vector<std::string>& lines, std::vector<std::string>& enabledLines,
                          const GeckoCode& gcode)
{
  if (gcode.enabled)
    enabledLines.push_back('$' + gcode.name);

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
  std::vector<std::string> enabledLines;

  for (const GeckoCode& geckoCode : gcodes)
  {
    SaveGeckoCode(lines, enabledLines, geckoCode);
  }

  inifile.SetLines("Gecko", lines);
  inifile.SetLines("Gecko_Enabled", enabledLines);
}
}
