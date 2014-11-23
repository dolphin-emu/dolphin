// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class IniFile;

namespace RmObjEngine
{

enum RmObjType
{
	RMOBJ_16BIT,
	RMOBJ_32BIT,
	RMOBJ_48BIT,
	RMOBJ_64BIT,
};

extern const char *RmObjTypeStrings[];

struct RmObjEntry
{
	RmObjEntry() {}
	RmObjEntry(RmObjType _t, u64 _value) : type(_t), value(_value) {}
	RmObjType type;
	u64 value;
};

struct RmObj
{
	std::string name;
	std::vector<RmObjEntry> entries;
	bool active;
	bool user_defined; // False if this code is shipped with Dolphin.
};

void LoadRmObjSection(const std::string& section, std::vector<RmObj> &patches,
                      IniFile &globalIni, IniFile &localIni);
void LoadRmObjs();
void ApplyRmObjs(const std::vector<RmObj> &rmobjects);
void Shutdown();

inline int GetRmObjTypeCharLength(RmObjType type)
{
	int size;
	switch (type)
	{
	case RmObjEngine::RMOBJ_16BIT:
		size = 4;
		break;

	case RmObjEngine::RMOBJ_32BIT:
		size = 8;
		break;

	case RmObjEngine::RMOBJ_48BIT:
		size = 12;
		break;

	case RmObjEngine::RMOBJ_64BIT:
		size = 16;
		break;
	}
	return size;
}

}  // namespace
extern std::vector<RmObjEngine::RmObj> rmObjCodes;
