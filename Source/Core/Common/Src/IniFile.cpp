// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.


// see IniFile.h

#include <stdlib.h>
#include <stdio.h>

#include <string>
#include <vector>
#include <iostream>
#include <fstream>
#include <algorithm>

#include "FileUtil.h"
#include "StringUtil.h"
#include "IniFile.h"

namespace {

void ParseLine(const std::string& line, std::string* keyOut, std::string* valueOut)
{
	if (line[0] == '#')
		return;

	int FirstEquals = (int)line.find("=", 0);

	if (FirstEquals >= 0)
	{
		// Yes, a valid line!
		*keyOut = StripSpaces(line.substr(0, FirstEquals));
		if (valueOut) *valueOut = StripQuotes(StripSpaces(line.substr(FirstEquals + 1, std::string::npos)));
	}
}

}

void IniFile::Section::Set(const char* key, const char* newValue)
{
	auto it = values.find(key);
	if (it != values.end())
		it->second = newValue;
	else
	{
		values[key] = newValue;
		keys_order.push_back(key);
	}
}

void IniFile::Section::Set(const char* key, const std::string& newValue, const std::string& defaultValue)
{
	if (newValue != defaultValue)
		Set(key, newValue);
	else
		Delete(key);
}

void IniFile::Section::Set(const char* key, const float newValue, const float defaultValue)
{
	if (newValue != defaultValue)
		Set(key, newValue);
	else
		Delete(key);
}

void IniFile::Section::Set(const char* key, int newValue, int defaultValue)
{
	if (newValue != defaultValue)
		Set(key, newValue);
	else
		Delete(key);
}

void IniFile::Section::Set(const char* key, bool newValue, bool defaultValue)
{
	if (newValue != defaultValue)
		Set(key, newValue);
	else
		Delete(key);
}

void IniFile::Section::Set(const char* key, const std::vector<std::string>& newValues)
{
	std::string temp;
	// Join the strings with ,
	std::vector<std::string>::const_iterator it;
	for (it = newValues.begin(); it != newValues.end(); ++it)
	{
		temp = (*it) + ",";
	}
	// remove last ,
	temp.resize(temp.length() - 1);
	Set(key, temp.c_str());
}

bool IniFile::Section::Get(const char* key, std::string* value, const char* defaultValue)
{
	auto it = values.find(key);
	if (it != values.end())
	{
		*value = it->second;
		return true;
	}
	else if (defaultValue)
	{
		*value = defaultValue;
		return true;
	}
	else
		return false;
}

bool IniFile::Section::Get(const char* key, std::vector<std::string>& out)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (!retval || temp.empty())
	{
		return false;
	}
	// ignore starting , if any
	size_t subStart = temp.find_first_not_of(",");
	size_t subEnd;

	// split by ,
	while (subStart != std::string::npos)
	{
		// Find next ,
		subEnd = temp.find_first_of(",", subStart);
		if (subStart != subEnd)
			// take from first char until next ,
			out.push_back(StripSpaces(temp.substr(subStart, subEnd - subStart)));
		// Find the next non , char
		subStart = temp.find_first_not_of(",", subEnd);
	}
	return true;
}

bool IniFile::Section::Get(const char* key, int* value, int defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (retval && TryParse(temp.c_str(), value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const char* key, u32* value, u32 defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const char* key, bool* value, bool defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (retval && TryParse(temp.c_str(), value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const char* key, float* value, float defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (retval && TryParse(temp.c_str(), value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const char* key, double* value, double defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp, 0);
	if (retval && TryParse(temp.c_str(), value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Exists(const char *key) const
{
	return values.find(key) != values.end();
}

bool IniFile::Section::Delete(const char *key)
{
	auto it = values.find(key);
	if (it == values.end())
		return false;

	values.erase(it);
	keys_order.erase(std::find(keys_order.begin(), keys_order.end(), key));
	return true;
}

// IniFile

const IniFile::Section* IniFile::GetSection(const char* sectionName) const
{
	for (const auto& sect : sections)
		if (!strcasecmp(sect.name.c_str(), sectionName))
			return (&(sect));
	return 0;
}

IniFile::Section* IniFile::GetSection(const char* sectionName)
{
	for (auto& sect : sections)
		if (!strcasecmp(sect.name.c_str(), sectionName))
			return (&(sect));
	return 0;
}

IniFile::Section* IniFile::GetOrCreateSection(const char* sectionName)
{
	Section* section = GetSection(sectionName);
	if (!section)
	{
		sections.push_back(Section(sectionName));
		section = &sections[sections.size() - 1];
	}
	return section;
}

bool IniFile::DeleteSection(const char* sectionName)
{
	Section* s = GetSection(sectionName);
	if (!s)
		return false;
	for (std::vector<Section>::iterator iter = sections.begin(); iter != sections.end(); ++iter)
	{
		if (&(*iter) == s)
		{
			sections.erase(iter);
			return true;
		}
	}
	return false;
}

bool IniFile::Exists(const char* sectionName, const char* key) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
		return false;
	return section->Exists(key);
}

void IniFile::SetLines(const char* sectionName, const std::vector<std::string> &lines)
{
	Section* section = GetOrCreateSection(sectionName);
	section->lines = lines;
}

bool IniFile::DeleteKey(const char* sectionName, const char* key)
{
	Section* section = GetSection(sectionName);
	if (!section)
		return false;
	return section->Delete(key);
}

// Return a list of all keys in a section
bool IniFile::GetKeys(const char* sectionName, std::vector<std::string>& keys) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
		return false;
	keys = section->keys_order;
	return true;
}

// Return a list of all lines in a section
bool IniFile::GetLines(const char* sectionName, std::vector<std::string>& lines, const bool remove_comments) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
		return false;

	lines.clear();
	for (std::string line : section->lines)
	{
		line = StripSpaces(line);

		if (remove_comments)
		{
			int commentPos = (int)line.find('#');
			if (commentPos == 0)
			{
				continue;
			}

			if (commentPos != (int)std::string::npos)
			{
				line = StripSpaces(line.substr(0, commentPos));
			}
		}

		lines.push_back(line);
	}

	return true;
}


void IniFile::SortSections()
{
	std::sort(sections.begin(), sections.end());
}

bool IniFile::Load(const char* filename, bool keep_current_data)
{
	// Maximum number of letters in a line
	static const int MAX_BYTES = 1024*32;

	if (!keep_current_data)
		sections.clear();
	// first section consists of the comments before the first real section

	// Open file
	std::ifstream in;
	OpenFStream(in, filename, std::ios::in);

	if (in.fail()) return false;

	Section* current_section = NULL;
	while (!in.eof())
	{
		char templine[MAX_BYTES];
		in.getline(templine, MAX_BYTES);
		std::string line = templine;

#ifndef _WIN32
		// Check for CRLF eol and convert it to LF
		if (!line.empty() && line.at(line.size()-1) == '\r')
		{
			line.erase(line.size()-1);
		}
#endif

		if (line.size() > 0)
		{
			if (line[0] == '[')
			{
				size_t endpos = line.find("]");

				if (endpos != std::string::npos)
				{
					// New section!
					std::string sub = line.substr(1, endpos - 1);
					current_section = GetOrCreateSection(sub.c_str());
				}
			}
			else
			{
				if (current_section)
				{
					std::string key, value;
					ParseLine(line, &key, &value);

					// Lines starting with '$', '*' or '+' are kept verbatim.
					// Kind of a hack, but the support for raw lines inside an
					// INI is a hack anyway.
					if ((key == "" && value == "")
					        || (line.size() >= 1 && (line[0] == '$' || line[0] == '+' || line[0] == '*')))
						current_section->lines.push_back(line.c_str());
					else
						current_section->Set(key, value.c_str());
				}
			}
		}
	}

	in.close();
	return true;
}

bool IniFile::Save(const char* filename)
{
	std::ofstream out;
	std::string temp = File::GetTempFilenameForAtomicWrite(filename);
	OpenFStream(out, temp, std::ios::out);

	if (out.fail())
	{
		return false;
	}

	for (auto& section : sections)
	{
		if (section.keys_order.size() != 0 || section.lines.size() != 0)
			out << "[" << section.name << "]" << std::endl;

		if (section.keys_order.size() == 0)
		{
			for (auto s : section.lines)
				out << s << std::endl;
		}
		else
		{
			for (auto kvit = section.keys_order.begin(); kvit != section.keys_order.end(); ++kvit)
			{
				auto pair = section.values.find(*kvit);
				out << pair->first << " = " << pair->second << std::endl;
			}
		}
	}

	out.close();

	return File::RenameSync(temp, filename);
}


bool IniFile::Get(const char* sectionName, const char* key, std::string* value, const char* defaultValue)
{
	Section* section = GetSection(sectionName);
	if (!section) {
		if (defaultValue) {
			*value = defaultValue;
		}
		return false;
	}
	return section->Get(key, value, defaultValue);
}

bool IniFile::Get(const char *sectionName, const char* key, std::vector<std::string>& values)
{
	Section *section = GetSection(sectionName);
	if (!section)
		return false;
	return section->Get(key, values);
}

bool IniFile::Get(const char* sectionName, const char* key, int* value, int defaultValue)
{
	Section *section = GetSection(sectionName);
	if (!section) {
		*value = defaultValue;
		return false;
	} else {
		return section->Get(key, value, defaultValue);
	}
}

bool IniFile::Get(const char* sectionName, const char* key, u32* value, u32 defaultValue)
{
	Section *section = GetSection(sectionName);
	if (!section) {
		*value = defaultValue;
		return false;
	} else {
		return section->Get(key, value, defaultValue);
	}
}

bool IniFile::Get(const char* sectionName, const char* key, bool* value, bool defaultValue)
{
	Section *section = GetSection(sectionName);
	if (!section) {
		*value = defaultValue;
		return false;
	} else {
		return section->Get(key, value, defaultValue);
	}
}


// Unit test. TODO: Move to the real unit test framework.
/*
   int main()
   {
    IniFile ini;
    ini.Load("my.ini");
    ini.Set("Hej", "A", "amaskdfl");
    ini.Set("Mossa", "A", "amaskdfl");
    ini.Set("Aissa", "A", "amaskdfl");
    //ini.Read("my.ini");
    std::string x;
    ini.Get("Hej", "B", &x, "boo");
    ini.DeleteKey("Mossa", "A");
    ini.DeleteSection("Mossa");
    ini.SortSections();
    ini.Save("my.ini");
    //UpdateVars(ini);
    return 0;
   }
 */
