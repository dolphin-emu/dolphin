// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

class IniFile;

namespace HideObjectEngine
{

enum HideObjectType
{
	HideObject_08BIT,
	HideObject_16BIT,
	HideObject_24BIT,
	HideObject_32BIT,
	HideObject_40BIT,
	HideObject_48BIT,
	HideObject_52BIT,
	HideObject_64BIT,
	HideObject_72BIT,
	HideObject_80BIT,
	HideObject_88BIT,
	HideObject_96BIT,
	HideObject_104BIT,
	HideObject_112BIT,
	HideObject_120BIT,
	HideObject_128BIT,
};

extern std::vector<std::string> HideObjectTypeStrings;

struct HideObjectEntry
{
	HideObjectEntry() {}
	HideObjectEntry(HideObjectType _t, u64 _value) : type(_t), value_lower(_value), value_upper(_value) {}
	HideObjectType type;
	u64 value_lower;
	u64 value_upper;
};

struct HideObject
{
	std::string name;
	std::vector<HideObjectEntry> entries;
	bool active;
	bool user_defined; // False if this code is shipped with Dolphin.
};

void LoadHideObjectSection(const std::string& section, std::vector<HideObject> &patches,
                      IniFile &globalIni, IniFile &localIni);
void LoadHideObjects();
void ApplyHideObjects(const std::vector<HideObject> &HideObjectects);
void ApplyFrameHideObjects();
void Shutdown();

inline int GetHideObjectTypeCharLength(HideObjectType type)
{
	return (type + 1) << 1;
}

}  // namespace
extern std::vector<HideObjectEngine::HideObject> HideObjectCodes;
