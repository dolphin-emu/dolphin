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

#ifndef _INIFILE_H_
#define _INIFILE_H_

#include "CommonTypes.h"

#include <fstream>
#include <map>
#include <string>
#include <sstream>

// some things that include IniFile.h rely on this being here
#include "StringUtil.h"

class IniFile;

class Section : public std::map<std::string, std::string>
{
	friend class IniFile;

public:
	Section() : m_use_lines(false) {}

	bool Exists(const std::string& key) const;
	void Delete(const std::string& key);

	void SetLines(const std::vector<std::string>& lines);
	void GetLines(std::vector<std::string>& lines);

	bool Get(const std::string& key, std::string* const val, const std::string& def = "") const;
	void Set(const std::string& key, const std::string& val, const std::string& def = "");

	template <typename V>
	void Set(const std::string& key, const V val)
	{
		std::ostringstream ss;
		ss << val;
		operator[](key) = ss.str();
	}

	// if val doesn't match def, set the key's value to val
	// otherwise delete that key
	//
	// this removed a lot of redundant code in the game-properties stuff
	template <typename V, typename D>
	void Set(const std::string& key, const V val, const D def)
	{
		if (val != def)
			Set(key, val);
		else
		{
			iterator f = find(key);
			if (f != end())
				erase(f);
		}
	}

	template <typename V>
	bool Get(const std::string& key, V* const val) const
	{
		const const_iterator f = find(key);
		if (f != end())
		{
			std::istringstream ss(f->second);
			ss >> *val;
			return true;
		}
		return false;
	}

	template <typename V, typename D>
	bool Get(const std::string& key, V* const val, const D def) const
	{
		if (Get(key, val))
			return true;
		*val = def;
		return false;
	}

protected:
	void Save(std::ostream& file) const;

	std::vector<std::string>	m_lines;

private:
	bool	m_use_lines;
};

class IniFile : public std::map<std::string, Section>
{
public:
	void Clean();
	bool Exists(const std::string& section) const;
	void Delete(const std::string& section);

	bool Save(const char filename[]) const;
	bool Load(const char filename[]);

	bool Save(const std::string& filename) const;
	bool Load(const std::string& filename);

	void Save(std::ostream& file) const;
	void Load(std::istream& file);
};

#endif // _INIFILE_H_
