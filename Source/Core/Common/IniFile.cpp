// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

// see IniFile.h

#include <algorithm>
#include <cstddef>
#include <cstring>
#include <fstream>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/FileUtil.h"
#include "Common/IniFile.h"
#include "Common/StringUtil.h"

void IniFile::ParseLine(const std::string& line, std::string* keyOut, std::string* valueOut)
{
	if (line[0] == '#')
		return;

	size_t firstEquals = line.find("=", 0);

	if (firstEquals != std::string::npos)
	{
		// Yes, a valid line!
		*keyOut = StripSpaces(line.substr(0, firstEquals));

		if (valueOut)
		{
			*valueOut = StripQuotes(StripSpaces(line.substr(firstEquals + 1, std::string::npos)));
		}
	}
}

const std::string& IniFile::NULL_STRING = "";

void IniFile::Section::Set(const std::string& key, const std::string& newValue)
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

void IniFile::Section::Set(const std::string& key, const std::string& newValue, const std::string& defaultValue)
{
	if (newValue != defaultValue)
		Set(key, newValue);
	else
		Delete(key);
}

void IniFile::Section::Set(const std::string& key, const std::vector<std::string>& newValues)
{
	std::string temp;
	// Join the strings with ,
	for (const std::string& value : newValues)
	{
		temp = value + ",";
	}
	// remove last ,
	temp.resize(temp.length() - 1);
	Set(key, temp);
}

bool IniFile::Section::Get(const std::string& key, std::string* value, const std::string& defaultValue)
{
	auto it = values.find(key);
	if (it != values.end())
	{
		*value = it->second;
		return true;
	}
	else if (&defaultValue != &NULL_STRING)
	{
		*value = defaultValue;
		return true;
	}
	else
		return false;
}

bool IniFile::Section::Get(const std::string& key, std::vector<std::string>* out)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (!retval || temp.empty())
	{
		return false;
	}
	// ignore starting , if any
	size_t subStart = temp.find_first_not_of(",");

	// split by ,
	while (subStart != std::string::npos)
	{
		// Find next ,
		size_t subEnd = temp.find_first_of(",", subStart);
		if (subStart != subEnd)
			// take from first char until next ,
			out->push_back(StripSpaces(temp.substr(subStart, subEnd - subStart)));
		// Find the next non , char
		subStart = temp.find_first_not_of(",", subEnd);
	}
	return true;
}

bool IniFile::Section::Get(const std::string& key, int* value, int defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const std::string& key, u32* value, u32 defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const std::string& key, bool* value, bool defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const std::string& key, float* value, float defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Get(const std::string& key, double* value, double defaultValue)
{
	std::string temp;
	bool retval = Get(key, &temp);
	if (retval && TryParse(temp, value))
		return true;
	*value = defaultValue;
	return false;
}

bool IniFile::Section::Exists(const std::string& key) const
{
	return values.find(key) != values.end();
}

bool IniFile::Section::Delete(const std::string& key)
{
	auto it = values.find(key);
	if (it == values.end())
		return false;

	values.erase(it);
	keys_order.erase(std::find(keys_order.begin(), keys_order.end(), key));
	return true;
}

// IniFile

const IniFile::Section* IniFile::GetSection(const std::string& sectionName) const
{
	for (const Section& sect : sections)
		if (!strcasecmp(sect.name.c_str(), sectionName.c_str()))
			return (&(sect));
	return nullptr;
}

IniFile::Section* IniFile::GetSection(const std::string& sectionName)
{
	for (Section& sect : sections)
		if (!strcasecmp(sect.name.c_str(), sectionName.c_str()))
			return (&(sect));
	return nullptr;
}

IniFile::Section* IniFile::GetOrCreateSection(const std::string& sectionName)
{
	Section* section = GetSection(sectionName);
	if (!section)
	{
		sections.push_back(Section(sectionName));
		section = &sections.back();
	}
	return section;
}

bool IniFile::DeleteSection(const std::string& sectionName)
{
	Section* s = GetSection(sectionName);
	if (!s)
		return false;
	for (auto iter = sections.begin(); iter != sections.end(); ++iter)
	{
		if (&(*iter) == s)
		{
			sections.erase(iter);
			return true;
		}
	}
	return false;
}

bool IniFile::Exists(const std::string& sectionName, const std::string& key) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
		return false;
	return section->Exists(key);
}

void IniFile::SetLines(const std::string& sectionName, const std::vector<std::string> &lines)
{
	Section* section = GetOrCreateSection(sectionName);
	section->lines = lines;
}

bool IniFile::DeleteKey(const std::string& sectionName, const std::string& key)
{
	Section* section = GetSection(sectionName);
	if (!section)
		return false;
	return section->Delete(key);
}

// Return a list of all keys in a section
bool IniFile::GetKeys(const std::string& sectionName, std::vector<std::string>* keys) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
	{
		return false;
	}
	*keys = section->keys_order;
	return true;
}

// Return a list of all lines in a section
bool IniFile::GetLines(const std::string& sectionName, std::vector<std::string>* lines, const bool remove_comments) const
{
	const Section* section = GetSection(sectionName);
	if (!section)
		return false;

	lines->clear();
	for (std::string line : section->lines)
	{
		line = StripSpaces(line);

		if (remove_comments)
		{
			size_t commentPos = line.find('#');
			if (commentPos == 0)
			{
				continue;
			}

			if (commentPos != std::string::npos)
			{
				line = StripSpaces(line.substr(0, commentPos));
			}
		}

		lines->push_back(line);
	}

	return true;
}

void IniFile::SortSections()
{
	sections.sort();
}

bool IniFile::Load(const std::string& filename, bool keep_current_data)
{
	if (!keep_current_data)
		sections.clear();
	// first section consists of the comments before the first real section

	// Open file
	std::ifstream in;
	OpenFStream(in, filename, std::ios::in);

	if (in.fail())
		return false;

	Section* current_section = nullptr;
	while (!in.eof())
	{
		std::string line;

		if (!std::getline(in, line))
		{
			if (in.eof())
				return true;
			else
				return false;
		}

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
					current_section = GetOrCreateSection(sub);
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
					if ((key == "" && value == "") ||
					    (line.size() >= 1 &&
					     (line[0] == '$' ||
					      line[0] == '+' ||
					      line[0] == '*')))
						current_section->lines.push_back(line);
					else
						current_section->Set(key, value);
				}
			}
		}
	}

	in.close();
	return true;
}

bool IniFile::Save(const std::string& filename)
{
	std::ofstream out;
	std::string temp = File::GetTempFilenameForAtomicWrite(filename);
	OpenFStream(out, temp, std::ios::out);

	if (out.fail())
	{
		return false;
	}

	for (const Section& section : sections)
	{
		if (section.keys_order.size() != 0 || section.lines.size() != 0)
			out << "[" << section.name << "]" << std::endl;

		if (section.keys_order.size() == 0)
		{
			for (const std::string& s : section.lines)
				out << s << std::endl;
		}
		else
		{
			for (const std::string& kvit : section.keys_order)
			{
				auto pair = section.values.find(kvit);
				out << pair->first << " = " << pair->second << std::endl;
			}
		}
	}

	out.close();

	return File::RenameSync(temp, filename);
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
