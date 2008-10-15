// Copyright (C) 2003-2008 Dolphin Project.

// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, version 2.0.

// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License 2.0 for more details.

// A copy of the GPL 2.0 should have been included with the program.
// If not, see http://www.gnu.org/licenses/

// Official SVN repository and contact information can be found at
// http://code.google.com/p/dolphin-emu/

struct AREntry {
	u32 cmd_addr;
	u32 value;
};

struct ARCode {
	std::string name;
	std::vector<AREntry> ops;
	bool active;
};

extern std::vector<ARCode> arCodes;

void RunActionReplayCode(const ARCode &arcode, bool nowIsBootup);
void LoadActionReplayCodes(IniFile &ini);
void DoARSubtype_RamWriteAndFill();
void DoARSubtype_WriteToPointer();
void DoARSubtype_AddCode();
void DoARSubtype_MasterCodeAndWriteToCCXXXXXX();
void DoARSubtype_Other();

