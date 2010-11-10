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

#include <stdlib.h>
#include <stdio.h>

#include "Common.h"
#include "CommonPaths.h"
#include "StringUtil.h"

// faster than sscanf
bool AsciiToHex(const char* _szValue, u32& result)
{
	char *endptr = NULL;
	const u32 value = strtoul(_szValue, &endptr, 16);

	if (!endptr || *endptr)
		return false;

	result = value;
	return true;
}

bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args)
{
	int writtenCount = vsnprintf(out, outsize, format, args);

	if (writtenCount > 0 && writtenCount < outsize)
	{
		out[writtenCount] = '\0';
		return true;
	}
	else
	{
		out[outsize - 1] = '\0';
		return false;
	}
}

std::string StringFromFormat(const char* format, ...)
{
	int writtenCount = -1;
	size_t newSize = strlen(format) + 4;
	char *buf = NULL;
	va_list args;
	while (writtenCount < 0)
	{
		delete[] buf;
		buf = new char[newSize + 1];
	
		va_start(args, format);
		writtenCount = vsnprintf(buf, newSize, format, args);
		va_end(args);

#ifndef _WIN32
		// vsnprintf does not return -1 on truncation in linux!
		// Instead it returns the size of the string we need.
		if (writtenCount >= (int)newSize)
			newSize = writtenCount;
#else
		newSize *= 2;
#endif
	}

	buf[writtenCount] = '\0';
	std::string temp = buf;
	delete[] buf;
	return temp;
}

// For Debugging. Read out an u8 array.
std::string ArrayToString(const u8 *data, u32 size, int line_len, bool spaces)
{
	std::ostringstream oss;
	oss << std::setfill('0') << std::hex;
	
	for (int line = 0; size; ++data, --size)
	{
		oss << std::setw(2) << (int)*data;
		
		if (line_len == ++line)
		{
			oss << '\n';
			line = 0;
		}
		else if (spaces)
			oss << ' ';
	}

	return oss.str();
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string &str)
{
	const size_t s = str.find_first_not_of(" \t\r\n");

	if (str.npos != s)
		return str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1);
	else
		return "";
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s)
{
	if (s.size() && '\"' == s[0] && '\"' == *s.rbegin())
		return s.substr(1, s.size() - 2);
	else
		return s;
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripNewline(const std::string& s)
{
	if (s.size() && '\n' == *s.rbegin())
		return s.substr(0, s.size() - 1);
	else
		return s;
}

bool TryParse(const std::string &str, u32 *const output)
{
	char *endptr = NULL;
	u32 value = strtoul(str.c_str(), &endptr, 0);
	
	if (!endptr || *endptr)
		return false;

	*output = value;
	return true;
}

bool TryParse(const std::string &str, bool *const output)
{
	if ('1' == str[0] || !strcasecmp("true", str.c_str()))
		*output = true;
	else if ('0' == str[0] || !strcasecmp("false", str.c_str()))
		*output = false;
	else
		return false;

	return true;
}

std::string StringFromInt(int value)
{
	char temp[16];
	sprintf(temp, "%i", value);
	return temp;
}

std::string StringFromBool(bool value)
{
	return value ? "True" : "False";
}

bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension)
{
	if (full_path.empty())
		return false;

	size_t dir_end = full_path.find_last_of("/"
	// windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
		":"
#endif
	);
	if (std::string::npos == dir_end)
		dir_end = 0;
	else
		dir_end += 1;

	size_t fname_end = full_path.rfind('.');
	if (fname_end < dir_end || std::string::npos == fname_end)
		fname_end = full_path.size();

	if (_pPath)
		*_pPath = full_path.substr(0, dir_end);

	if (_pFilename)
		*_pFilename = full_path.substr(dir_end, fname_end - dir_end);

	if (_pExtension)
		*_pExtension = full_path.substr(fname_end);

	return true;
}

std::string PathToFilename(const std::string &Path)
{
	std::string Name, Ending;
	SplitPath(Path, 0, &Name, &Ending);
	return Name + Ending;
}

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path, const std::string& _Filename)
{
	_CompleteFilename = _Path;

	// check for seperator
	if (DIR_SEP_CHR != *_CompleteFilename.rbegin())
		_CompleteFilename += DIR_SEP_CHR;

	// add the filename
	_CompleteFilename += _Filename;
}

void SplitString(const std::string& str, const char delim, std::vector<std::string>& output)
{
	std::istringstream iss(str);
	output.resize(1);

	while (std::getline(iss, *output.rbegin(), delim))
		output.push_back("");

	output.pop_back();
}

std::string TabsToSpaces(int tab_size, const std::string &in)
{
	const std::string spaces(tab_size, ' ');
	std::string out(in);

	size_t i = 0;
	while (out.npos != (i = out.find('\t')))
		out.replace(i, 1, spaces);

	return out;
}
