// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GeckoCodeConfig.h"

#include "StringUtil.h"

#include <vector>
#include <string>
#include <sstream>

namespace Gecko
{

void LoadCodes(const IniFile& globalIni, const IniFile& localIni, std::vector<GeckoCode>& gcodes)
{
	const IniFile* inis[] = {&globalIni, &localIni};
	for (size_t i = 0; i < ArraySize(inis); ++i)
	{
		std::vector<std::string> lines;
		inis[i]->GetLines("Gecko", lines, false);

		GeckoCode gcode;

		for (auto& line : lines)
		{
			if (line.empty())
				continue;

			std::istringstream	ss(line);

			switch ((line)[0])
			{

				// enabled or disabled code
			case '+' :
				ss.seekg(1);
			case '$' :
				if (gcode.name.size())
					gcodes.push_back(gcode);
				gcode = GeckoCode();
				gcode.enabled = (1 == ss.tellg());	// silly
				gcode.user_defined = i == 1;
				ss.seekg(1, std::ios_base::cur);
				// read the code name
				std::getline(ss, gcode.name, '[');	// stop at [ character (beginning of contributor name)
				gcode.name = StripSpaces(gcode.name);
				// read the code creator name
				std::getline(ss, gcode.creator, ']');
				break;

				// notes
			case '*':
				gcode.notes.push_back(std::string(++line.begin(), line.end()));
				break;

				// either part of the code, or an option choice
			default :
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
			gcodes.push_back(gcode);

		inis[i]->GetLines("Gecko_Enabled", lines, false);

		for (auto line : lines)
		{
			if (line.size() == 0 || line[0] != '$')
				continue;
			std::string name = line.substr(1);
			for (auto& ogcode : gcodes)
			{
				if (ogcode.name == name)
					ogcode.enabled = true;
			}
		}
	}
}

// used by the SaveGeckoCodes function
void SaveGeckoCode(std::vector<std::string>& lines, std::vector<std::string>& enabledLines, const GeckoCode& gcode)
{
	if (gcode.enabled)
		enabledLines.push_back("$" + gcode.name);

	if (!gcode.user_defined)
		return;

	std::string name;

	// save the name
	name += '$';
	name += gcode.name;

	// save the creator name
	if (gcode.creator.size())
	{
		name += " [";
		name += gcode.creator;
		name += ']';
	}

	lines.push_back(name);

	// save all the code lines
	std::vector<GeckoCode::Code>::const_iterator
		codes_iter = gcode.codes.begin(),
		codes_end = gcode.codes.end();
	for (; codes_iter!=codes_end; ++codes_iter)
	{
		//ss << std::hex << codes_iter->address << ' ' << codes_iter->data;
		//lines.push_back(StringFromFormat("%08X %08X", codes_iter->address, codes_iter->data));
		lines.push_back(codes_iter->original_line);
		//ss.clear();
	}

	// save the notes
	std::vector<std::string>::const_iterator
		notes_iter = gcode.notes.begin(),
		notes_end = gcode.notes.end();
	for (; notes_iter!=notes_end; ++notes_iter)
		lines.push_back(std::string("*") + *notes_iter);
}

void SaveCodes(IniFile& inifile, const std::vector<GeckoCode>& gcodes)
{
	std::vector<std::string> lines;
	std::vector<std::string> enabledLines;

	std::vector<GeckoCode>::const_iterator
		gcodes_iter = gcodes.begin(),
		gcodes_end = gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		SaveGeckoCode(lines, enabledLines, *gcodes_iter);
	}

	inifile.SetLines("Gecko", lines);
	inifile.SetLines("Gecko_Enabled", enabledLines);
}

};
