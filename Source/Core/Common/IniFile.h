// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <list>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

template<typename T, typename T2 = T> class Option;

class OptionGroup
{
public:
	class _OptionBase
	{
	public:
		virtual ~_OptionBase() {}
		virtual bool Set(const std::string& value) const = 0;
		virtual std::string Get() const = 0;
		virtual const std::string& GetKey() const = 0;
		virtual bool SetDefault() const = 0;
	};

	template<typename T, typename T2> class _Option : public _OptionBase
	{
	public:
		_Option(Option<T, T2>* option, const std::string& key, const T& defaultValue)
		: m_option(option), m_key(key), m_defaultValue(defaultValue) {}

		bool Set(const std::string& value) const override
		{
			T2 tmp;
			if (TryParse<T2>(value, &tmp) && m_option->Get() != (T)tmp)
			{
				*m_option = (T)tmp;
				return true;
			}
			return false;
		}

		std::string Get() const override
		{
			return ToString<T2>((T2)m_option->Get());
		}

		const std::string& GetKey() const override
		{
			return m_key;
		}

		bool SetDefault() const override
		{
			if (m_option->Get() != m_defaultValue)
			{
				*m_option = m_defaultValue;
				return true;
			}
			return false;
		}

	private:
		Option<T, T2>* const m_option;
		const std::string m_key;
		const T m_defaultValue;
	};

	OptionGroup(const char* name) : m_name(name) {}

	const OptionGroup& operator=(const OptionGroup& group)
	{
		return *this;
	}

	template<typename T, typename T2> void AddOption(Option<T, T2>* option, const std::string& key, const T& defaultValue)
	{
		m_options.emplace_back(new _Option<T, T2>(option, key, defaultValue));
	}

	const char* m_name;
	std::vector<std::unique_ptr<_OptionBase>> m_options;
};

template<typename T, typename T2> class Option
{
public:
	Option(OptionGroup& group, const std::string& key, const T& defaultValue)
	: m_value(defaultValue)
	{
		group.AddOption<T, T2>(this, key, defaultValue);
	}

	operator T&()
	{
		return m_value;
	}

	T& Get()
	{
		return m_value;
	}

	void Set(const T& value)
	{
		m_value = value;
	}

	const Option<T, T2>& operator=(const T& value)
	{
		m_value = value;
		return *this;
	}

private:
	T m_value;
};

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

		bool Exists(const std::string& key) const;
		bool Delete(const std::string& key);

		void Set(const std::string& key, const std::string& newValue);
		void Set(const std::string& key, const std::string& newValue, const std::string& defaultValue);

		void Set(const std::string& key, u32 newValue)
		{
			Set(key, ToString(newValue));
		}

		void Set(const std::string& key, float newValue)
		{
			Set(key, ToString(newValue));
		}

		void Set(const std::string& key, double newValue)
		{
			Set(key, ToString(newValue));
		}

		void Set(const std::string& key, int newValue)
		{
			Set(key, ToString(newValue));
		}

		void Set(const std::string& key, bool newValue)
		{
			Set(key, ToString(newValue));
		}

		void Set(const OptionGroup::_OptionBase& option)
		{
			Set(option.GetKey(), option.Get());
		}

		template<typename T>
		void Set(const std::string& key, T newValue, const T defaultValue)
		{
			if (newValue != defaultValue)
				Set(key, newValue);
			else
				Delete(key);
		}

		void Set(const std::string& key, const std::vector<std::string>& newValues);

		bool Get(const std::string& key, int* value, int defaultValue = 0);
		bool Get(const std::string& key, u32* value, u32 defaultValue = 0);
		bool Get(const std::string& key, bool* value, bool defaultValue = false);
		bool Get(const std::string& key, float* value, float defaultValue = false);
		bool Get(const std::string& key, double* value, double defaultValue = false);
		bool Get(const std::string& key, std::string* value, const std::string& defaultValue = NULL_STRING);
		bool Get(const std::string& key, std::vector<std::string>* values);

		template<typename T> bool Get(const std::string& key, Option<T>* value, Option<T> defaultValue)
		{
			return Get(key, &(T&)(*value), (T)defaultValue);
		}
		template<typename T> bool Get(const std::string& key, Option<T>* value)
		{
			return Get(key, &(T&)(*value));
		}

		bool Get(const OptionGroup::_OptionBase& option);

		bool operator < (const Section& other) const
		{
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
	bool Load(const std::string& filename, bool keep_current_data = false);

	bool Save(const std::string& filename);

	// Returns true if key exists in section
	bool Exists(const std::string& sectionName, const std::string& key) const;

	template<typename T> bool GetIfExists(const std::string& sectionName, const std::string& key, T value)
	{
		if (Exists(sectionName, key))
			return GetOrCreateSection(sectionName)->Get(key, value);

		return false;
	}

	bool GetKeys(const std::string& sectionName, std::vector<std::string>* keys) const;

	void SetLines(const std::string& sectionName, const std::vector<std::string>& lines);
	bool GetLines(const std::string& sectionName, std::vector<std::string>* lines, const bool remove_comments = true) const;

	bool DeleteKey(const std::string& sectionName, const std::string& key);
	bool DeleteSection(const std::string& sectionName);

	void SortSections();

	Section* GetOrCreateSection(const std::string& section);

	// This function is related to parsing data from lines of INI files
	// It's used outside of IniFile, which is why it is exposed publicly
	// In particular it is used in PostProcessing for its configuration
	static void ParseLine(const std::string& line, std::string* keyOut, std::string* valueOut);

private:
	std::list<Section> sections;

	const Section* GetSection(const std::string& section) const;
	Section* GetSection(const std::string& section);
	std::string* GetLine(const std::string& section, const std::string& key);
	void CreateSection(const std::string& section);

	static const std::string& NULL_STRING;
};
