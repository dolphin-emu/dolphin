// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class IniFile;

namespace RmObjEngine
{

enum RmObjType
{
	RMOBJ_08BIT,
	RMOBJ_16BIT,
	RMOBJ_24BIT,
	RMOBJ_32BIT,
	RMOBJ_40BIT,
	RMOBJ_48BIT,
	RMOBJ_52BIT,
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
void ApplyFrameRmObjs();
void Shutdown();

inline int GetRmObjTypeCharLength(RmObjType type)
{	
	return (type + 1) << 1;
}

}  // namespace
extern std::vector<RmObjEngine::RmObj> rmObjCodes;
