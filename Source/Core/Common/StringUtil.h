// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <charconv>
#include <cstdarg>
#include <cstddef>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <limits>
#include <locale>
#include <span>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>

#include "Common/CommonTypes.h"

namespace detail
{
template <typename T>
constexpr bool IsBooleanEnum()
{
  if constexpr (std::is_enum_v<T>)
  {
    return std::is_same_v<std::underlying_type_t<T>, bool>;
  }
  else
  {
    return false;
  }
}
}  // namespace detail

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

std::string_view StripWhitespace(std::string_view s);
std::string_view StripSpaces(std::string_view s);
std::string_view StripQuotes(std::string_view s);

std::string ReplaceAll(std::string result, std::string_view src, std::string_view dest);

void ReplaceBreaksWithSpaces(std::string& str);

void TruncateToCString(std::string* s);

bool TryParse(const std::string& str, bool* output);

template <typename T>
requires(std::is_integral_v<T> ||
         (std::is_enum_v<T> && !detail::IsBooleanEnum<T>())) bool TryParse(const std::string& str,
                                                                           T* output, int base = 0)
{
  char* end_ptr = nullptr;

  // Set errno to a clean slate.
  errno = 0;

  // Read a u64 for unsigned types and s64 otherwise.
  using ReadType = std::conditional_t<std::is_unsigned_v<T>, u64, s64>;
  ReadType value;

  if constexpr (std::is_unsigned_v<T>)
    value = std::strtoull(str.c_str(), &end_ptr, base);
  else
    value = std::strtoll(str.c_str(), &end_ptr, base);

  // Fail if the end of the string wasn't reached.
  if (end_ptr == nullptr || *end_ptr != '\0')
    return false;

  // Fail if the value was out of 64-bit range.
  if (errno == ERANGE)
    return false;

  using LimitsType = typename std::conditional_t<std::is_enum_v<T>, std::underlying_type<T>,
                                                 std::common_type<T>>::type;
  // Fail if outside numeric limits.
  if (value < std::numeric_limits<LimitsType>::min() ||
      value > std::numeric_limits<LimitsType>::max())
  {
    return false;
  }

  *output = static_cast<T>(value);
  return true;
}

template <typename T>
requires(detail::IsBooleanEnum<T>()) bool TryParse(const std::string& str, T* output)
{
  bool value;
  if (!TryParse(str, &value))
    return false;

  *output = static_cast<T>(value);
  return true;
}

template <typename T, std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
bool TryParse(std::string str, T* const output)
{
  // Replace commas with dots.
  std::istringstream iss(ReplaceAll(std::move(str), ",", "."));

  // Use "classic" locale to force a "dot" decimal separator.
  iss.imbue(std::locale::classic());

  T tmp;

  // Succeed if a value was read and the entire string was used.
  if (iss >> tmp && iss.eof())
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

namespace Common
{
template <typename T, typename std::enable_if_t<std::is_integral_v<T>>* = nullptr>
std::from_chars_result FromChars(std::string_view sv, T& value, int base = 10)
{
  const char* const first = sv.data();
  const char* const last = first + sv.size();
  return std::from_chars(first, last, value, base);
}
template <typename T, typename std::enable_if_t<std::is_floating_point_v<T>>* = nullptr>
std::from_chars_result FromChars(std::string_view sv, T& value,
                                 std::chars_format fmt = std::chars_format::general)
{
  const char* const first = sv.data();
  const char* const last = first + sv.size();
  return std::from_chars(first, last, value, fmt);
}
};  // namespace Common

std::string TabsToSpaces(int tab_size, std::string str);

std::vector<std::string> SplitString(const std::string& str, char delim);
std::string JoinStrings(const std::vector<std::string>& strings, const std::string& delimiter);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
// This requires forward slashes to be used for the path separators, even on Windows.
bool SplitPath(std::string_view full_path, std::string* path, std::string* filename,
               std::string* extension);

// Converts the path separators of a path into forward slashes on Windows, which is assumed to be
// true for paths at various places in the codebase.
void UnifyPathSeparators(std::string& path);
std::string WithUnifiedPathSeparators(std::string path);

// Extracts just the filename (including extension) from a full path.
// This requires forward slashes to be used for the path separators, even on Windows.
std::string PathToFileName(std::string_view path);

void StringPopBackIf(std::string* s, char c);
size_t StringUTF8CodePointCount(std::string_view str);

std::string CP1252ToUTF8(std::string_view str);
std::string SHIFTJISToUTF8(std::string_view str);
std::string UTF8ToSHIFTJIS(std::string_view str);
std::string WStringToUTF8(std::wstring_view str);
std::string UTF16BEToUTF8(const char16_t* str, size_t max_size);  // Stops at \0
std::string UTF16ToUTF8(std::u16string_view str);
std::u16string UTF8ToUTF16(std::string_view str);

#ifdef _WIN32

std::wstring UTF8ToWString(std::string_view str);

#ifdef _UNICODE
inline std::string TStrToUTF8(std::wstring_view str)
{
  return WStringToUTF8(str);
}

inline std::wstring UTF8ToTStr(std::string_view str)
{
  return UTF8ToWString(str);
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

std::filesystem::path StringToPath(std::string_view path);
std::string PathToString(const std::filesystem::path& path);

namespace Common
{
/// Returns whether a character is printable, i.e. whether 0x20 <= c <= 0x7e is true.
/// Use this instead of calling std::isprint directly to ensure
/// the C locale is being used and to avoid possibly undefined behaviour.
inline bool IsPrintableCharacter(char c)
{
  return std::isprint(c, std::locale::classic());
}

/// Returns whether a character is a letter, i.e. whether 'a' <= c <= 'z' || 'A' <= c <= 'Z'
/// is true. Use this instead of calling std::isalpha directly to ensure
/// the C locale is being used and to avoid possibly undefined behaviour.
inline bool IsAlpha(char c)
{
  return std::isalpha(c, std::locale::classic());
}

inline char ToLower(char ch)
{
  return std::tolower(ch, std::locale::classic());
}

inline char ToUpper(char ch)
{
  return std::toupper(ch, std::locale::classic());
}

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
  return WStringToUTF8(stream.str());
#else
  return stream.str();
#endif
}

#ifdef _WIN32
std::vector<std::string> CommandLineToUtf8Argv(const wchar_t* command_line);
#endif

std::string GetEscapedHtml(std::string html);

void ToLower(std::string* str);
void ToUpper(std::string* str);
bool CaseInsensitiveEquals(std::string_view a, std::string_view b);
std::string BytesToHexString(std::span<const u8> bytes);
}  // namespace Common
