// Copyright (C) 2003 Dolphin Project.

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

// Thanks to Treeki for writing the original class - 29/01/2012

#ifndef _SETTINGS_HANDLER_H
#define _SETTINGS_HANDLER_H

#include <string>

#include "Common.h"
#include "../CoreParameter.h"
#define SETTINGS_SIZE	0x100

class SettingsHandler
{
public:
	SettingsHandler();

	void AddSetting(const char *key, const char *value);

	const u8 *GetData() const;
	const std::string GetValue(std::string key);

	void Decrypt();
	void Reset();
	std::string generateSerialNumber();
private:
	void WriteByte(u8 b);

	u8 m_buffer[SETTINGS_SIZE];
	u32 m_position, m_key;
	std::string decoded;
};

#endif
