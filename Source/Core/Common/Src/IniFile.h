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

#include <string>
#include <vector>

#include "StringUtil.h"

class IniFile
{
public:
	class Section
	{
		friend class IniFile;

	public:
		Section() {}
		Section(const std::string& _name) : name(_name) {}

		bool Exists(const char *key) const;
		bool Delete(const char *key);

		std::string* GetLine(const char* key, std::string* valueOut, std::string* commentOut);
		void Set(const char* key, const char* newValue);
		void Set(const char* key, const std::string& newValue, const std::string& defaultValue);

		void Set(const std::string &key, const std::string &value) {
			Set(key.c_str(), value.c_str());
		}
		bool Get(const char* key, std::string* value, const char* defaultValue);

		void Set(const char* key, u32 newValue) {
			Set(key, StringFromFormat("0x%08x", newValue).c_str());
		}
		void Set(const char* key, float newValue) {
			Set(key, StringFromFormat("%f", newValue).c_str());
		}
		void Set(const char* key, const float newValue, const float defaultValue);
		void Set(const char* key, double newValue) {
			Set(key, StringFromFormat("%f", newValue).c_str());
		}
		
		void Set(const char* key, int newValue, int defaultValue);
		void Set(const char* key, int newValue) {
			Set(key, StringFromInt(newValue).c_str());
		}
		
		void Set(const char* key, bool newValue, bool defaultValue);
		void Set(const char* key, bool newValue) {
			Set(key, StringFromBool(newValue).c_str());
		}
		void Set(const char* key, const std::vector<std::string>& newValues);

		bool Get(const char* key, int* value, int defaultValue = 0);
		bool Get(const char* key, u32* value, u32 defaultValue = 0);
		bool Get(const char* key, bool* value, bool defaultValue = false);
		bool Get(const char* key, float* value, float defaultValue = false);
		bool Get(const char* key, double* value, double defaultValue = false);
		bool Get(const char* key, std::vector<std::string>& values);

		bool operator < (const Section& other) const {
			return name < other.name;
		}

	protected:
		std::vector<std::string> lines;
		std::string name;
		std::string comment;
	};

	bool Load(const char* filename);
	bool Load(const std::string &filename) { return Load(filename.c_str()); }
	bool Save(const char* filename);
	bool Save(const std::string &filename) { return Save(filename.c_str()); }

	// Returns true if key exists in section
	bool Exists(const char* sectionName, const char* key) const;

	// TODO: Get rid of these, in favor of the Section ones.
	void Set(const char* sectionName, const char* key, const char* newValue) {
		GetOrCreateSection(sectionName)->Set(key, newValue);
	}
	void Set(const char* sectionName, const char* key, const std::string& newValue) {
		GetOrCreateSection(sectionName)->Set(key, newValue.c_str());
	}
	void Set(const char* sectionName, const char* key, int newValue) {
		GetOrCreateSection(sectionName)->Set(key, newValue);
	}
	void Set(const char* sectionName, const char* key, u32 newValue) {
		GetOrCreateSection(sectionName)->Set(key, newValue);
	}
	void Set(const char* sectionName, const char* key, bool newValue) {
		GetOrCreateSection(sectionName)->Set(key, newValue);
	}
	void Set(const char* sectionName, const char* key, const std::vector<std::string>& newValues) {
		GetOrCreateSection(sectionName)->Set(key, newValues);
	}

	// TODO: Get rid of these, in favor of the Section ones.
	bool Get(const char* sectionName, const char* key, std::string* value, const char* defaultValue = "");
	bool Get(const char* sectionName, const char* key, int* value, int defaultValue = 0);
	bool Get(const char* sectionName, const char* key, u32* value, u32 defaultValue = 0);
	bool Get(const char* sectionName, const char* key, bool* value, bool defaultValue = false);
	bool Get(const char* sectionName, const char* key, std::vector<std::string>& values);

	template<typename T> bool GetIfExists(const char* sectionName, const char* key, T value)
	{
		if (Exists(sectionName, key))
			return Get(sectionName, key, value);
		return false;
	}

	bool GetKeys(const char* sectionName, std::vector<std::string>& keys) const;

	void SetLines(const char* sectionName, const std::vector<std::string> &lines);
	bool GetLines(const char* sectionName, std::vector<std::string>& lines, const bool remove_comments = true) const;

	bool DeleteKey(const char* sectionName, const char* key);
	bool DeleteSection(const char* sectionName);

	void SortSections();

	Section* GetOrCreateSection(const char* section);

private:
	std::vector<Section> sections;

	const Section* GetSection(const char* section) const;
	Section* GetSection(const char* section);
	std::string* GetLine(const char* section, const char* key);
	void CreateSection(const char* section);
};

#endif // _INIFILE_H_
