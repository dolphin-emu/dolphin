// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdarg>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"

#ifdef _MSC_VER
#include <filesystem>
#define HAS_STD_FILESYSTEM
#endif

std::string StringFromFormatV(const char* format, va_list args);

std::string StringFromFormat(const char* format, ...)
#if !defined _WIN32
    // On compilers that support function attributes, this gives StringFromFormat
    // the same errors and warnings that printf would give.
    __attribute__((__format__(printf, 1, 2)))
#endif
    ;

// Cheap!
bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args);

template <size_t Count>
inline void CharArrayFromFormat(char (&out)[Count], const char* format, ...)
{
  va_list args;
  va_start(args, format);
  CharArrayFromFormatV(out, Count, format, args);
  va_end(args);
}

// Good
std::string ArrayToString(const u8* data, u32 size, int line_len = 20, bool spaces = true);

std::string_view StripSpaces(std::string_view s);
std::string_view StripQuotes(std::string_view s);

bool TryParse(const std::string& str, bool* output);
bool TryParse(const std::string& str, u16* output);
bool TryParse(const std::string& str, u32* output);
bool TryParse(const std::string& str, u64* output);

template <typename N>
static bool TryParse(const std::string& str, N* const output)
{
  std::istringstream iss(str);
  // is this right? not doing this breaks reading floats on locales that use different decimal
  // separators
  iss.imbue(std::locale("C"));

  N tmp;
  if (iss >> tmp)
  {
    *output = tmp;
    return true;
  }

  return false;
}

template <typename N>
bool TryParseVector(const std::string& str, std::vector<N>* output, const char delimiter = ',')
{
  output->clear();
  std::istringstream buffer(str);
  std::string variable;

  while (std::getline(buffer, variable, delimiter))
  {
    N tmp = 0;
    if (!TryParse(variable, &tmp))
      return false;
    output->push_back(tmp);
  }
  return true;
}

std::string ValueToString(u16 value);
std::string ValueToString(u32 value);
std::string ValueToString(u64 value);
std::string ValueToString(float value);
std::string ValueToString(double value);
std::string ValueToString(int value);
std::string ValueToString(s64 value);
std::string ValueToString(bool value);
template <typename T, std::enable_if_t<std::is_enum<T>::value>* = nullptr>
std::string ValueToString(T value)
{
  return ValueToString(static_cast<std::underlying_type_t<T>>(value));
}

// Generates an hexdump-like representation of a binary data blob.
std::string HexDump(const u8* data, size_t size);

// TODO: kill this
bool AsciiToHex(const std::string& _szValue, u32& result);

std::string TabsToSpaces(int tab_size, std::string str);

std::vector<std::string> SplitString(const std::string& str, char delim);
std::string JoinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SplitPath(std::string_view full_path, std::string* path, std::string* filename,
               std::string* extension);

void BuildCompleteFilename(std::string& complete_filename, std::string_view path,
                           std::string_view filename);
std::string ReplaceAll(std::string result, std::string_view src, std::string_view dest);

bool StringBeginsWith(std::string_view str, std::string_view begin);
bool StringEndsWith(std::string_view str, std::string_view end);
void StringPopBackIf(std::string* s, char c);

std::string CP1252ToUTF8(std::string_view str);
std::string SHIFTJISToUTF8(std::string_view str);
std::string UTF8ToSHIFTJIS(std::string_view str);
std::string UTF16ToUTF8(std::wstring_view str);
std::string UTF16BEToUTF8(const char16_t* str, size_t max_size);  // Stops at \0

#ifdef _WIN32

std::wstring UTF8ToUTF16(std::string_view str);

#ifdef _UNICODE
inline std::string TStrToUTF8(std::wstring_view str)
{
  return UTF16ToUTF8(str);
}

inline std::wstring UTF8ToTStr(std::string_view str)
{
  return UTF8ToUTF16(str);
}
#else
inline std::string TStrToUTF8(std::string_view str)
{
  return str;
}

inline std::string UTF8ToTStr(std::string_view str)
{
  return str;
}
#endif

#endif

#ifdef HAS_STD_FILESYSTEM
std::filesystem::path StringToPath(std::string_view path);
std::string PathToString(const std::filesystem::path& path);
#endif

// Thousand separator. Turns 12345678 into 12,345,678
template <typename I>
std::string ThousandSeparate(I value, int spaces = 0)
{
#ifdef _WIN32
  std::wostringstream stream;
#else
  std::ostringstream stream;
#endif

  stream << std::setw(spaces) << value;

#ifdef _WIN32
  return UTF16ToUTF8(stream.str());
#else
  return stream.str();
#endif
}
