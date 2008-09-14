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

#include <stdlib.h>
#include <stdio.h>

#include "StringUtil.h"
#include "TestFramework.h"

// faster than sscanf
bool AsciiToHex(const char* _szValue, u32& result)
{
	u32 value = 0;
	size_t finish = strlen(_szValue);

	if (finish > 8)
	{
		finish = 8;
	}

	for (size_t count = 0; count < finish; count++)
	{
		value <<= 4;

		switch (_szValue[count])
		{
		    case '0':
			    break;

		    case '1':
			    value += 1;
			    break;

		    case '2':
			    value += 2;
			    break;

		    case '3':
			    value += 3;
			    break;

		    case '4':
			    value += 4;
			    break;

		    case '5':
			    value += 5;
			    break;

		    case '6':
			    value += 6;
			    break;

		    case '7':
			    value += 7;
			    break;

		    case '8':
			    value += 8;
			    break;

		    case '9':
			    value += 9;
			    break;

		    case 'A':
			    value += 10;
			    break;

		    case 'a':
			    value += 10;
			    break;

		    case 'B':
			    value += 11;
			    break;

		    case 'b':
			    value += 11;
			    break;

		    case 'C':
			    value += 12;
			    break;

		    case 'c':
			    value += 12;
			    break;

		    case 'D':
			    value += 13;
			    break;

		    case 'd':
			    value += 13;
			    break;

		    case 'E':
			    value += 14;
			    break;

		    case 'e':
			    value += 14;
			    break;

		    case 'F':
			    value += 15;
			    break;

		    case 'f':
			    value += 15;
			    break;

		    default:
			    return false;
			    break;
		}
	}

	result = value;
	return (true);
}


bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args)
{
	int writtenCount = vsnprintf(out, outsize, format, args);

	if (writtenCount > 0)
	{
		out[writtenCount] = '\0';
		return(true);
	}
	else
	{
		out[outsize - 1] = '\0';
		return(false);
	}
}


// Expensive!
void StringFromFormatV(std::string* out, const char* format, va_list args)
{
	int writtenCount = -1;
	size_t newSize = strlen(format) + 16;
	char* buf = 0;

	while (writtenCount < 0)
	{
		delete [] buf;
		buf = new char[newSize + 1];
		writtenCount = vsnprintf(buf, newSize, format, args);
		// ARGH! vsnprintf does no longer return -1 on truncation in newer libc!
		// WORKAROUND! let's fake the old behaviour (even though it's less efficient).
		// TODO: figure out why the fix causes an invalid read in strlen called from vsnprintf :(
//		if (writtenCount >= (int)newSize)
//			writtenCount = -1;
		newSize *= 2;
	}

	buf[writtenCount] = '\0';
	*out = buf;
	delete[] buf;
}


std::string StringFromFormat(const char* format, ...)
{
	std::string temp;
	va_list args;
	va_start(args, format);
	StringFromFormatV(&temp, format, args);
	va_end(args);
	return(temp);
}


void ToStringFromFormat(std::string* out, const char* format, ...)
{
	va_list args;
	va_start(args, format);
	StringFromFormatV(out, format, args);
	va_end(args);
}


// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(std::string s)
{
	int i;

	for (i = 0; i < (int)s.size(); i++)
	{
		if ((s[i] != ' ') && (s[i] != 9))
		{
			break;
		}
	}

	s = s.substr(i);

	for (i = (int)s.size() - 1; i > 0; i--)
	{
		if ((s[i] != ' ') && (s[i] != 9))
		{
			break;
		}
	}

	return(s.substr(0, i + 1));
}


// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s)
{
	if ((s[0] == '\"') && (s[s.size() - 1] == '\"'))
	{
		return(s.substr(1, s.size() - 2));
	}
	else
	{
		return(s);
	}
}


bool TryParseInt(const char* str, int* outVal)
{
	const char* s = str;
	int value = 0;
    bool negativ = false;

    if (*s == '-')
    {
        negativ = true;
        s++;
    }

	while (*s)
	{
		char c = *s++;

		if ((c < '0') || (c > '9'))
		{
			return(false);
		}

		value = value * 10 + (c - '0');
	}
    if (negativ)
        value = -value;

	*outVal = value;
	return(true);
}


bool TryParseBool(const char* str, bool* output)
{
	if ((str[0] == '1') || !strcmp(str, "true") || !strcmp(str, "True") || !strcmp(str, "TRUE"))
	{
		*output = true;
		return(true);
	}
	else if (str[0] == '0' || !strcmp(str, "false") || !strcmp(str, "False") || !strcmp(str, "FALSE"))
	{
		*output = false;
		return(true);
	}

	return(false);
}


std::string StringFromInt(int value)
{
	char temp[16];
	sprintf(temp, "%i", value);
	return(std::string(temp));
}


std::string StringFromBool(bool value)
{
	return(value ? "True" : "False");
}


#ifdef _WIN32
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension)
{
	char drive[_MAX_DRIVE];
	char dir[_MAX_DIR];
	char fname[_MAX_FNAME];
	char ext[_MAX_EXT];

	if (_splitpath_s(full_path.c_str(), drive, _MAX_DRIVE, dir, _MAX_DIR, fname, _MAX_FNAME, ext, _MAX_EXT) == 0)
	{
		if (_pPath)
		{
			*_pPath = std::string(drive) + std::string(dir);
		}

		if (_pFilename != 0)
		{
			*_pFilename = fname;
		}

		if (_pExtension != 0)
		{
			*_pExtension = ext;
		}

		return(true);
	}

	return(false);
}


#else
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension)
{
	size_t last_slash = full_path.rfind('/');

	if (last_slash == std::string::npos)
	{
		return(false);
	}

	size_t last_dot = full_path.rfind('.');

	if ((last_dot == std::string::npos) || (last_dot < last_slash))
	{
		return(false);
	}

	if (_pPath)
	{
		*_pPath = full_path.substr(0, last_slash + 1);
	}

	if (_pFilename)
	{
		*_pFilename = full_path.substr(last_slash + 1, last_dot - (last_slash + 1));
	}

	if (_pExtension)
	{
		*_pExtension = full_path.substr(last_dot + 1);
		_pExtension->insert(0, ".");
	}
	else if (_pFilename)
	{
		*_pFilename += full_path.substr(last_dot);
	}

	return(true);
}


#endif

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path, const std::string& _Filename)
{
	_CompleteFilename = _Path;

	// check for seperator
	if (_CompleteFilename[_CompleteFilename.size() - 1] != '\\')
	{
		_CompleteFilename += _T("\\");
	}

	// add the filename
	_CompleteFilename += _Filename;
}


void SplitString(const std::string& str, const std::string& delim, std::vector<std::string>& output)
{
	output.clear();

	size_t offset = 0;
	size_t delimIndex = 0;

	delimIndex = str.find(delim, offset);

	while (delimIndex != std::string::npos)
	{
		output.push_back(str.substr(offset, delimIndex - offset));
		offset += delimIndex - offset + delim.length();
		delimIndex = str.find(delim, offset);
	}

	output.push_back(str.substr(offset));
}


bool TryParseUInt(const std::string& str, u32* output)
{
	if (!strcmp(str.substr(0, 2).c_str(), "0x") || !strcmp(str.substr(0, 2).c_str(), "0X"))
	{
		return(sscanf(str.c_str() + 2, "%x", output) > 0);
	}
	else
	{
		return(sscanf(str.c_str(), "%d", output) > 0);
	}
}


int ChooseStringFrom(const char* str, const char* * items)
{
	int i = 0;

	while (items[i] != 0)
	{
		if (!strcmp(str, items[i]))
		{
			return(i);
		}

		i++;
	}

	return(-1);
}


// Hmm, sometimes this test doesn't get run :P
TEST(splitStringTest)
{
	/*
	   std::string orig = "abc:def";
	   std::vector<std::string> split;
	   SplitString(orig, std::string(":"), split);
	   CHECK(split.size() == 2);
	   CHECK(!strcmp(split[0].c_str(), "abc"));
	   CHECK(!strcmp(split[1].c_str(), "def"));

	   orig = "abc";
	   SplitString(orig, std::string(":"), split);
	   CHECK(split.size() == 1);
	   orig = ":";
	   SplitString(orig, std::string(":"), split);
	   CHECK(split.size() == 2);*/
}
