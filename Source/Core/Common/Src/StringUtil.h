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

#ifndef _STRINGUTIL_H_
#define _STRINGUTIL_H_

#include <stdarg.h>

#include <vector>
#include <string>

#include "Common.h"

std::string StringFromFormat(const char* format, ...);
void ToStringFromFormat(std::string* out, const char* format, ...);

// WARNING - only call once with a set of args!
void StringFromFormatV(std::string* out, const char* format, va_list args);
// Cheap!
bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args);

// Good
std::string ArrayToString(const u8 *data, u32 size, u32 offset = 0, int line_len = 20, bool Spaces = true);


template<size_t Count>
inline void CharArrayFromFormat(char (& out)[Count], const char* format, ...)
{
	va_list args;
	va_start(args, format);
	CharArrayFromFormatV(out, Count, format, args);
	va_end(args);
}


std::string StripSpaces(const std::string &s);
std::string StripQuotes(const std::string &s);
std::string StripNewline(const std::string &s);
std::string ThS(int a, bool b = true); // thousand separator


std::string StringFromInt(int value);
std::string StringFromBool(bool value);

bool TryParseInt(const char* str, int* outVal);
bool TryParseBool(const char* str, bool* output);
bool TryParseUInt(const std::string& str, u32* output);


// TODO: kill this
bool AsciiToHex(const char* _szValue, u32& result);
u32 Ascii2Hex(std::string _Text);
std::string Hex2Ascii(u32 _Text);

std::string TabsToSpaces(int tab_size, const std::string &in);

void SplitString(const std::string& str, const std::string& delim, std::vector<std::string>& output);
int ChooseStringFrom(const char* str, const char* * items);


// filehelper
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename, std::string* _pExtension);
void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path, const std::string& _Filename);
void NormalizeDirSep(std::string* str);

#endif // _STRINGUTIL_H_
