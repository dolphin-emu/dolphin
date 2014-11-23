// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// RmObjEngine
// Supports the removal of objects/effects from the rendering loop

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonPaths.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

#include "Core/ConfigManager.h"
#include "Core/RmObjEngine.h"

using namespace Common;

namespace RmObjEngine
{

	const char *RmObjTypeStrings[] =
	{
		"16bits",
		"32bits",
		"48bits",
		"64bits"
	};

	static std::vector<RmObj> rmObjCodes;

	void LoadRmObjSection(const std::string& section, std::vector<RmObj>& rmobjects, IniFile& globalIni, IniFile& localIni)
	{
		// Load the name of all enabled rmobjects
		std::string enabledSectionName = section + "_Enabled";
		std::vector<std::string> enabledLines;
		std::set<std::string> enabledNames;
		localIni.GetLines(enabledSectionName, &enabledLines);
		for (const std::string& line : enabledLines)
		{
			if (line.size() != 0 && line[0] == '$')
			{
				std::string name = line.substr(1, line.size() - 1);
				enabledNames.insert(name);
			}
		}

		const IniFile* inis[2] = { &globalIni, &localIni };

		for (const IniFile* ini : inis)
		{
			std::vector<std::string> lines;
			RmObj currentRmObj;
			ini->GetLines(section, &lines);

			for (std::string& line : lines)
			{
				if (line.size() == 0)
					continue;

				if (line[0] == '$')
				{
					// Take care of the previous code
					if (currentRmObj.name.size())
					{
						rmobjects.push_back(currentRmObj);
					}
					currentRmObj.entries.clear();

					// Set active and name
					currentRmObj.name = line.substr(1, line.size() - 1);
					currentRmObj.active = enabledNames.find(currentRmObj.name) != enabledNames.end();
					currentRmObj.user_defined = (ini == &localIni);
				}
				else
				{
					std::string::size_type loc = line.find_first_of('=', 0);

					if (loc != std::string::npos)
					{
						line[loc] = ':';
					}

					std::vector<std::string> items;
					SplitString(line, ':', items);

					if (items.size() >= 2)
					{
						RmObjEntry pE;
						bool success = true;
						success &= TryParse(items[1], &pE.value);

						pE.type = RmObjType(std::find(RmObjTypeStrings, RmObjTypeStrings + 4, items[0]) - RmObjTypeStrings);
						success &= (pE.type != (RmObjType)4);
						if (success)
						{
							currentRmObj.entries.push_back(pE);
						}
					}
				}
			}

			if (currentRmObj.name.size() && currentRmObj.entries.size())
			{
				rmobjects.push_back(currentRmObj);
			}
		}
	}

	void LoadRmObjs()
	{
		IniFile merged = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadGameIni();
		IniFile globalIni = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadDefaultGameIni();
		IniFile localIni = SConfig::GetInstance().m_LocalCoreStartupParameter.LoadLocalGameIni();

		LoadRmObjSection("RmObjCodes", rmObjCodes, globalIni, localIni);
	}

	void ApplyRmObjs(const std::vector<RmObj> &rmobjects)
	{
		// Make the rendering code skip over checking skip entries
		//SConfig::GetInstance().m_LocalCoreStartupParameter.update = false;

		// Wait until the next time the next time the rendering thread finishes checking the skip entries.
		//while (SConfig::GetInstance().m_LocalCoreStartupParameter.done == false);

		SConfig::GetInstance().m_LocalCoreStartupParameter.render_skip_entries.clear();

		for (const RmObj& rmobject : rmobjects)
		{
			if (rmobject.active)
			{
				for (const RmObjEntry& entry : rmobject.entries)
				{
					u64 value = entry.value;
					SkipEntry skipEntry;
					int size = GetRmObjTypeCharLength(entry.type) >> 1;

					for (int j = size; j > 0; j--){
						skipEntry.push_back((0xFF & (value >> ((j - 1)*8))));
					}

					SConfig::GetInstance().m_LocalCoreStartupParameter.render_skip_entries.push_back(skipEntry);

					//Paper Mario - Black Bar Skip
					//skipEntry.push_back(0xFE);
					//skipEntry.push_back(0xD0);
					//skipEntry.push_back(0x00);
					//skipEntry.push_back(0xF0);

					//Metroid - Black Bar Skip
					//skipEntry.push_back(0xC3);
					//skipEntry.push_back(0xA5);
					//skipEntry.push_back(0x00);
					//skipEntry.push_back(0x00);
				}
			}
		}
		SConfig::GetInstance().m_LocalCoreStartupParameter.num_render_skip_entries = SConfig::GetInstance().m_LocalCoreStartupParameter.render_skip_entries.size();
		//SConfig::GetInstance().m_LocalCoreStartupParameter.update = true;
	}

	void Shutdown()
	{
		rmObjCodes.clear();
	}

}  // namespace
