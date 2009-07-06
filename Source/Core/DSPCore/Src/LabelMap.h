// Copyright (C) 2003-2009 Dolphin Project.

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

#ifndef _LABELMAP_H
#define _LABELMAP_H

#include <string>
#include <vector>

#include "Common.h"

enum LabelType
{
	LABEL_IADDR = 1,  // Jump addresses, etc
	LABEL_DADDR = 2,  // Data addresses, etc
	LABEL_VALUE = 4,
	LABEL_ANY = 0xFF,
};

class LabelMap
{
	struct label_t
	{
		label_t(const std::string &lbl, s32 address, LabelType ltype) : name(lbl), addr(address), type(ltype) {}
		std::string name;
		s32	addr;
		LabelType type;
	};
	std::vector<label_t> labels;

public:
	LabelMap();
	~LabelMap() { }
	void RegisterDefaults();
	void RegisterLabel(const std::string &label, u16 lval, LabelType type = LABEL_VALUE);
	void DeleteLabel(const std::string &label);
	bool GetLabelValue(const std::string &label, u16 *value, LabelType type = LABEL_ANY) const;
	void Clear();
};

#endif  // _LABELMAP_H
