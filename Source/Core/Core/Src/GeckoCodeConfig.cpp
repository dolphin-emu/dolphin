// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "GeckoCodeConfig.h"

#include "StringUtil.h"

#include <vector>
#include <string>
#include <sstream>

#define GECKO_CODE_INI_SECTION	"Gecko"

namespace Gecko
{

void LoadCodes(const IniFile& inifile, std::vector<GeckoCode>& gcodes)
{
	std::vector<std::string> lines;
	inifile.GetLines(GECKO_CODE_INI_SECTION, lines, false);

	GeckoCode gcode;

	std::vector<std::string>::const_iterator
		lines_iter = lines.begin(),
		lines_end = lines.end();
	for (; lines_iter!=lines_end; ++lines_iter)
	{
		if (lines_iter->empty())
			continue;

		std::istringstream	ss(*lines_iter);

		switch ((*lines_iter)[0])
		{

			// enabled or disabled code
		case '+' :
			ss.seekg(1);
		case '$' :
			if (gcode.name.size())
				gcodes.push_back(gcode);
			gcode = GeckoCode();
			gcode.enabled = (1 == ss.tellg());	// silly
			ss.seekg(1, std::ios_base::cur);
			// read the code name
			std::getline(ss, gcode.name, '[');	// stop at [ character (beginning of contributor name)
			gcode.name = StripSpaces(gcode.name);
			// read the code creator name
			std::getline(ss, gcode.creator, ']');
			break;

			// notes
		case '*':
			gcode.notes.push_back(std::string(++lines_iter->begin(), lines_iter->end()));
			break;

			// either part of the code, or an option choice
		default :
		{
			GeckoCode::Code new_code;
			// TODO: support options
			new_code.original_line = *lines_iter;
			ss >> std::hex >> new_code.address >> new_code.data;
			gcode.codes.push_back(new_code);
		}
			break;
		}

	}

	// add the last code
	if (gcode.name.size())
		gcodes.push_back(gcode);
}

// used by the SaveGeckoCodes function
void SaveGeckoCode(std::vector<std::string>& lines, const GeckoCode& gcode)
{
	std::string name;

	if (gcode.enabled)
		name += '+';

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

	std::vector<GeckoCode>::const_iterator
		gcodes_iter = gcodes.begin(),
		gcodes_end = gcodes.end();
	for (; gcodes_iter!=gcodes_end; ++gcodes_iter)
	{
		SaveGeckoCode(lines, *gcodes_iter);
	}

	inifile.SetLines(GECKO_CODE_INI_SECTION, lines);
}

};
