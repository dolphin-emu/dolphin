// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// RmObjEngine
// Supports the removal of objects/effects from the rendering loop

#include "Core/ConfigManager.h"
#include "Core/Core.h"
#include "Core/RmObjEngine.h"

namespace RmObjEngine
{

	const char *RmObjTypeStrings[] =
	{
		"8bits",
		"16bits",
		"24bits",
		"32bits",
		"40bits",
		"48bits",
		"56bits",
		"64bits",
		"72bits",
		"80bits",
		"88bits",
		"96bits",
		"104bits",
		"112bits",
		"120bits",
		"128bits"
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

					if (items.size() >= 3)
					{
						RmObjEntry pE;
						bool success = true;
						success &= TryParse(items[1], &pE.value_upper);
						success &= TryParse(items[2], &pE.value_lower);

						pE.type = RmObjType(std::find(RmObjTypeStrings, RmObjTypeStrings + 16, items[0]) - RmObjTypeStrings);
						success &= (pE.type != (RmObjType)16);
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
		SConfig::GetInstance().m_LocalCoreStartupParameter.update = false;

		// Wait until the next time the next time the rendering thread finishes checking the skip entries.
		if (Core::IsRunning())
			while (SConfig::GetInstance().m_LocalCoreStartupParameter.done == false) {}

		SConfig::GetInstance().m_LocalCoreStartupParameter.object_removal_codes.clear();
		SConfig::GetInstance().m_LocalCoreStartupParameter.num_object_skip_data_bytes.clear();

		for (const RmObj& rmobject : rmobjects)
		{
			if (rmobject.active)
			{
				for (const RmObjEntry& entry : rmobject.entries)
				{
					u64 value_add_lower = entry.value_lower;
					u64 value_add_upper = entry.value_upper;
					SkipEntry skipEntry;
					int size = GetRmObjTypeCharLength(entry.type) >> 1;

					//Save the size of each entry, so it doesn't have to be calculated over and over during the rendering loop.
					SConfig::GetInstance().m_LocalCoreStartupParameter.num_object_skip_data_bytes.push_back(size);

					if (size > 8) 
					{
						size = size - 8;
						for (int j = size; j > 0; --j)
						{
							skipEntry.push_back((0xFF & (value_add_upper >> ((j - 1) * 8))));
						}
						size = 8;
					}
					
					for (int j = size; j > 0; --j)
					{
						skipEntry.push_back((0xFF & (value_add_lower >> ((j - 1) * 8))));
					}
					

					SConfig::GetInstance().m_LocalCoreStartupParameter.object_removal_codes.push_back(skipEntry);
				}
			}
		}
		SConfig::GetInstance().m_LocalCoreStartupParameter.num_object_removal_codes = SConfig::GetInstance().m_LocalCoreStartupParameter.object_removal_codes.size();
		SConfig::GetInstance().m_LocalCoreStartupParameter.update = true;
	}

	void ApplyFrameRmObjs()
	{
		ApplyRmObjs(rmObjCodes);
	}

	void Shutdown()
	{
		rmObjCodes.clear();
	}

}  // namespace
