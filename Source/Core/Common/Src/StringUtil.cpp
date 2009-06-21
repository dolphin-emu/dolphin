// Copyright (C) 2003-2009 Dolphin Project.

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

// faster than sscanf
bool AsciiToHex(const char* _szValue, u32& result)
{
	u32 value = 0;
	size_t finish = strlen(_szValue);

	if (finish > 8)
		finish = 8;  // Max 32-bit values are supported.

	for (size_t count = 0; count < finish; count++)
	{
		value <<= 4;
		switch (_szValue[count])
		{
		    case '0': break;
		    case '1': value += 1; break;
		    case '2': value += 2; break;
		    case '3': value += 3; break;
		    case '4': value += 4; break;
		    case '5': value += 5; break;
		    case '6': value += 6; break;
		    case '7': value += 7; break;
		    case '8': value += 8; break;
		    case '9': value += 9; break;
		    case 'A':
		    case 'a': value += 10; break;
		    case 'B':
		    case 'b': value += 11; break;
		    case 'C':
		    case 'c': value += 12; break;
		    case 'D':
		    case 'd': value += 13; break;
		    case 'E':
		    case 'e': value += 14; break;
		    case 'F':
		    case 'f': value += 15; break;
		    default:
			    return false;
			    break;
		}
	}

	result = value;
	return (true);
}

// Convert AB to it's ascii table entry numbers 0x4142
u32 Ascii2Hex(std::string _Text)
{
	// Reset the return value zero
	u32 Result = 0;

	// Max 32-bit values are supported
	size_t Length = _Text.length();
	if (Length > 4)
		Length = 4;

	for (int i = 0; i < (int)Length; i++)
	{
		// Add up the values, for example RSPE becomes, 0x52000000, then 0x52530000 and so on
		Result += _Text.c_str()[i] << ((Length - 1 - i) * 8);
	}
	// Return the value
	return Result;
}

// Convert it back again
std::string Hex2Ascii(u32 _Text)
{
	// Create temporary storate
	char Result[5];  // need space for the final \0
	// Go through the four characters
	sprintf(Result, "%c%c%c%c", _Text >> 24, _Text >> 16, _Text >> 8, _Text);
	// Return the string
	std::string StrResult = Result;
	return StrResult;
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

// Expensive!
void ToStringFromFormat(std::string* out, const char* format, ...)
{
	int writtenCount = -1;
	int newSize = (int)strlen(format) + 4;
	char *buf = 0;
	va_list args;
	while (writtenCount < 0)
	{
		delete [] buf;
		buf = new char[newSize + 1];
	
	    va_start(args, format);
		writtenCount = vsnprintf(buf, newSize, format, args);
		va_end(args);
		if (writtenCount >= (int)newSize) {
			writtenCount = -1;
		}
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
	int writtenCount = -1;
	int newSize = (int)strlen(format) + 4;
	char *buf = 0;
	va_list args;
	while (writtenCount < 0)
	{
		delete [] buf;
		buf = new char[newSize + 1];
	
	    va_start(args, format);
		writtenCount = vsnprintf(buf, newSize, format, args);
		va_end(args);
		if (writtenCount >= (int)newSize) {
			writtenCount = -1;
		}
		// ARGH! vsnprintf does no longer return -1 on truncation in newer libc!
		// WORKAROUND! let's fake the old behaviour (even though it's less efficient).
		// TODO: figure out why the fix causes an invalid read in strlen called from vsnprintf :(
//		if (writtenCount >= (int)newSize)
//			writtenCount = -1;
		newSize *= 2;
	}

	buf[writtenCount] = '\0';
	std::string temp = buf;
	return temp;
}


// For Debugging. Read out an u8 array.
std::string ArrayToString(const u8 *data, u32 size, u32 offset, int line_len, bool Spaces)
{
	std::string Tmp, Spc;
	if (Spaces) Spc = " "; else Spc = "";
	for (u32 i = 0; i < size; i++)
	{
		Tmp += StringFromFormat("%02x%s", data[i + offset], Spc.c_str());
		if(i > 1 && (i + 1) % line_len == 0) Tmp.append("\n"); // break long lines
	}	
	return Tmp;
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string &str)
{
	std::string s = str;
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

	return s.substr(0, i + 1);
}


// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s)
{
	if ((s[0] == '\"') && (s[s.size() - 1] == '\"'))
		return s.substr(1, s.size() - 2);
	else
		return s;
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripNewline(const std::string& s)
{
	if (!s.size())
		return s;
	else if (s[s.size() - 1] == '\n')
		return s.substr(0, s.size() - 1);
	else
		return s;
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
			return false;
		}

		value = value * 10 + (c - '0');
	}
    if (negativ)
        value = -value;

	*outVal = value;
	return true;
}


bool TryParseBool(const char* str, bool* output)
{
	if ((str[0] == '1') || !strcmp(str, "true") || !strcmp(str, "True") || !strcmp(str, "TRUE"))
	{
		*output = true;
		return true;
	}
	else if (str[0] == '0' || !strcmp(str, "false") || !strcmp(str, "False") || !strcmp(str, "FALSE"))
	{
		*output = false;
		return true;
	}
	return false;
}

std::string StringFromInt(int value)
{
	char temp[16];
	sprintf(temp, "%i", value);
	return std::string(temp);
}

std::string StringFromBool(bool value)
{
	return value ? "True" : "False";
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

		return true;
	}

	return false;
}


#else
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension)
{
	size_t last_slash = full_path.rfind('/');

	if (last_slash == std::string::npos)
	{
		return false; // FIXME return the filename
	}

	size_t last_dot = full_path.rfind('.');

	if ((last_dot == std::string::npos) || (last_dot < last_slash))
	{
		return false; // FIXME why missing . is critical?
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

	return true;
}


#endif

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path, const std::string& _Filename)
{
	_CompleteFilename = _Path;

	// check for seperator
	if (_CompleteFilename[_CompleteFilename.size() - 1] != DIR_SEP_CHR)
	{
		_CompleteFilename += DIR_SEP_CHR;
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
		return sscanf(str.c_str() + 2, "%x", output) > 0;
	else
		return sscanf(str.c_str(), "%d", output) > 0;
}


int ChooseStringFrom(const char* str, const char* * items)
{
	int i = 0;
	while (items[i] != 0)
	{
		if (!strcmp(str, items[i]))
			return i;
		i++;
	}
	return -1;
}


// Thousand separator. Turns 12345678 into 12,345,678.
std::string ThS(int Integer, bool Unsigned)
{
	// Create storage space
	char cbuf[20];
	// Determine treatment of signed or unsigned
	if(Unsigned) sprintf(cbuf, "%u", Integer); else sprintf(cbuf, "%i", Integer);

	std::string sbuf = cbuf;
	for (u32 i = 0; i < sbuf.length(); ++i)
	{
		if((i & 3) == 3)
		{
			sbuf.insert(sbuf.length() - i, ",");
		}
	}
	return sbuf;
}

void NormalizeDirSep(std::string* str)
{
#ifdef _WIN32
	int i;
	while ((i = (int)str->find_first_of('\\')) >= 0)
	{
		str->replace(i, 1, DIR_SEP);
	}
#endif
}

std::string TabsToSpaces(int tab_size, const std::string &in)
{
	std::string out;
	int len = 0;
	// First, compute the size of the new string.
	for (int i = 0; i < in.size(); i++)
	{
		if (in[i] == '\t')
			len += tab_size;
		else
			len += 1;
	}
	out.resize(len);
	int out_ctr = 0;
	for (int i = 0; i < in.size(); i++)
	{
		if (in[i] == '\t')
		{
			for (int j = 0; j < tab_size; j++)
				out[out_ctr++] = ' ';
		} 
		else 
		{
			out[out_ctr++] = in[i];
		}
	}
	return out;
}