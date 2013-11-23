// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


#ifndef _INIFILE_H_
#define _INIFILE_H_

#include <map>
#include <string>
#include <set>
#include <vector>

#include "StringUtil.h"

struct CaseInsensitiveStringCompare
{
	bool operator() (const std::string& a, const std::string& b) const
	{
		return strcasecmp(a.c_str(), b.c_str()) < 0;
	}
};

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
		std::string name;

		std::vector<std::string> keys_order;
		std::map<std::string, std::string, CaseInsensitiveStringCompare> values;

		std::vector<std::string> lines;
	};

	/**
	 * Loads sections and keys.
	 * @param filename filename of the ini file which should be loaded
	 * @param keep_current_data If true, "extends" the currently loaded list of sections and keys with the loaded data (and replaces existing entries). If false, existing data will be erased.
	 * @warning Using any other operations than "Get*" and "Exists" is untested and will behave unexpectedly
	 * @todo This really is just a hack to support having two levels of gameinis (defaults and user-specified) and should eventually be replaced with a less stupid system.
	 */
	bool Load(const char* filename, bool keep_current_data = false);
	bool Load(const std::string &filename, bool keep_current_data = false) { return Load(filename.c_str(), keep_current_data); }

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
