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

#ifndef _INIFILE_H
#define _INIFILE_H

#include <string>
#include <vector>

#include "StringUtil.h"

class Section
{
public:
	Section();
	Section(const std::string& _name);
	Section(const Section& other);
	std::vector<std::string>lines;
	std::string name;
	std::string comment;

	bool operator<(const Section& other) const
	{
		return(name < other.name);
	}
};

class IniFile
{
public:
	IniFile();
	~IniFile();

	bool Load(const char* filename);
	bool Save(const char* filename);

	void Set(const char* sectionName, const char* key, const char* newValue);
	void Set(const char* sectionName, const char* key, int newValue);
	void Set(const char* sectionName, const char* key, u32 newValue);
	void Set(const char* sectionName, const char* key, bool newValue);
	void Set(const char* sectionName, const char* key, const std::string& newValue) {Set(sectionName, key, newValue.c_str());}
	void Set(const char* sectionName, const char* key, const std::vector<std::string>& newValues);

	void SetLines(const char* sectionName, const std::vector<std::string> &lines);

	// getter should be const
	bool Get(const char* sectionName, const char* key, std::string* value, const char* defaultValue = "");
	bool Get(const char* sectionName, const char* key, int* value, int defaultValue = 0);
	bool Get(const char* sectionName, const char* key, u32* value, u32 defaultValue = 0);
	bool Get(const char* sectionName, const char* key, bool* value, bool defaultValue = false);
	bool Get(const char* sectionName, const char* key, std::vector<std::string>& values);

	bool GetKeys(const char* sectionName, std::vector<std::string>& keys) const;
	bool GetLines(const char* sectionName, std::vector<std::string>& lines) const;

	bool DeleteKey(const char* sectionName, const char* key);
	bool DeleteSection(const char* sectionName);

	void SortSections();

	void ParseLine(const std::string& line, std::string* keyOut, std::string* valueOut, std::string* commentOut) const;
	std::string* GetLine(Section* section, const char* key, std::string* valueOut, std::string* commentOut);

private:
	std::vector<Section>sections;

	const Section* GetSection(const char* section) const;
	Section* GetSection(const char* section);
	Section* GetOrCreateSection(const char* section);
	std::string* GetLine(const char* section, const char* key);
	void CreateSection(const char* section);
};

#endif
