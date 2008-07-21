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
/////////////////////////////////////////////////////////////////////////////////////////////////////
// M O D U L E  B E G I N ///////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

#pragma once

/////////////////////////////////////////////////////////////////////////////////////////////////////
// C L A S S ////////////////////////////////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////////////////////////

class IniFile
{
public:
	IniFile(void);
	~IniFile(void);
	
	void SetFile(const TCHAR *fname);
	void SetSection(const TCHAR *section);

	int  ReadInt    (const TCHAR *key, int def = 0);
	void WriteInt   (const TCHAR *key, int value);
	bool ReadBool   (const TCHAR *key, bool def = false);
	void WriteBool  (const TCHAR *key, bool value);
	void ReadString (const TCHAR *key, const TCHAR *def, TCHAR *out, int size = 255);
	void WriteString(const TCHAR *key, const TCHAR *value);
	void ReadStringList (const TCHAR *key, std::vector<std::string> &list);
	void WriteStringList(const TCHAR *key, std::vector<std::string> &list);

private:
	TCHAR filename[512];
	TCHAR section[256];
};