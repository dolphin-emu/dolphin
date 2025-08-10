// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/StringUtil.h"

#include <algorithm>
#include <array>
#include <codecvt>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <istream>
#include <iterator>
#include <limits.h>
#include <locale>
#include <sstream>
#include <string>
#include <type_traits>
#include <vector>
#include <unordered_map>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonPaths.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/Swap.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
constexpr u32 CODEPAGE_SHIFT_JIS = 932;
constexpr u32 CODEPAGE_WINDOWS_1252 = 1252;
#else
#include <errno.h>
#include <iconv.h>
#include <locale.h>
#endif

#if !defined(_WIN32) && !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) &&       \
    !defined(__NetBSD__)
static locale_t GetCLocale()
{
  static locale_t c_locale = newlocale(LC_ALL_MASK, "C", nullptr);
  return c_locale;
}
#endif

std::string HexDump(const u8* data, size_t size)
{
  constexpr size_t BYTES_PER_LINE = 16;

  std::string out;
  for (size_t row_start = 0; row_start < size; row_start += BYTES_PER_LINE)
  {
    out += fmt::format("{:06x}: ", row_start);
    for (size_t i = 0; i < BYTES_PER_LINE; ++i)
    {
      if (row_start + i < size)
      {
        out += fmt::format("{:02x} ", data[row_start + i]);
      }
      else
      {
        out += "   ";
      }
    }
    out += " ";
    for (size_t i = 0; i < BYTES_PER_LINE; ++i)
    {
      if (row_start + i < size)
      {
        char c = static_cast<char>(data[row_start + i]);
        out += Common::IsPrintableCharacter(c) ? c : '.';
      }
    }
    out += "\n";
  }
  return out;
}

bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args)
{
  int writtenCount;

#ifdef _WIN32
  // You would think *printf are simple, right? Iterate on each character,
  // if it's a format specifier handle it properly, etc.
  //
  // Nooooo. Not according to the C standard.
  //
  // According to the C99 standard (7.19.6.1 "The fprintf function")
  //     The format shall be a multibyte character sequence
  //
  // Because some character encodings might have '%' signs in the middle of
  // a multibyte sequence (SJIS for example only specifies that the first
  // byte of a 2 byte sequence is "high", the second byte can be anything),
  // printf functions have to decode the multibyte sequences and try their
  // best to not screw up.
  //
  // Unfortunately, on Windows, the locale for most languages is not UTF-8
  // as we would need. Notably, for zh_TW, Windows chooses EUC-CN as the
  // locale, and completely fails when trying to decode UTF-8 as EUC-CN.
  //
  // On the other hand, the fix is simple: because we use UTF-8, no such
  // multibyte handling is required as we can simply assume that no '%' char
  // will be present in the middle of a multibyte sequence.
  //
  // This is why we look up the default C locale here and use _vsnprintf_l.
  static _locale_t c_locale = nullptr;
  if (!c_locale)
    c_locale = _create_locale(LC_ALL, "C");
  writtenCount = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  locale_t previousLocale = uselocale(GetCLocale());
#endif
  writtenCount = vsnprintf(out, outsize, format, args);
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  uselocale(previousLocale);
#endif
#endif

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
  va_list args;
  va_start(args, format);
  std::string res = StringFromFormatV(format, args);
  va_end(args);
  return res;
}

std::string StringFromFormatV(const char* format, va_list args)
{
  char* buf = nullptr;
#ifdef _WIN32
  int required = _vscprintf(format, args);
  buf = new char[required + 1];
  CharArrayFromFormatV(buf, required + 1, format, args);

  std::string temp = buf;
  delete[] buf;
#else
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  locale_t previousLocale = uselocale(GetCLocale());
#endif
  if (vasprintf(&buf, format, args) < 0)
  {
    ERROR_LOG_FMT(COMMON, "Unable to allocate memory for string");
    buf = nullptr;
  }

#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  uselocale(previousLocale);
#endif

  std::string temp = buf;
  free(buf);
#endif
  return temp;
}

// For Debugging. Read out an u8 array.
std::string ArrayToString(const u8* data, u32 size, int line_len, bool spaces)
{
  std::ostringstream oss;
  oss << std::setfill('0') << std::hex;

  for (int line = 0; size; ++data, --size)
  {
    oss << std::setw(2) << static_cast<int>(*data);

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

template <typename T>
static std::string_view StripEnclosingChars(std::string_view str, T chars)
{
  const size_t s = str.find_first_not_of(chars);

  if (str.npos != s)
    return str.substr(s, str.find_last_not_of(chars) - s + 1);
  else
    return "";
}

// Turns "\n\r\t hello " into "hello" (trims at the start and end but not inside).
std::string_view StripWhitespace(std::string_view str)
{
  return StripEnclosingChars(str, " \t\r\n");
}

std::string_view StripSpaces(std::string_view str)
{
  return StripEnclosingChars(str, ' ');
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripWhitespace above, for example.
std::string_view StripQuotes(std::string_view s)
{
  if (!s.empty() && '\"' == s[0] && '\"' == *s.rbegin())
    return s.substr(1, s.size() - 2);
  else
    return s;
}

// Turns "\n\rhello" into "  hello".
void ReplaceBreaksWithSpaces(std::string& str)
{
  std::ranges::replace(str, '\r', ' ');
  std::ranges::replace(str, '\n', ' ');
}

void TruncateToCString(std::string* s)
{
  const size_t terminator = s->find_first_of('\0');
  if (terminator != s->npos)
    s->resize(terminator);
}

bool TryParse(const std::string& str, bool* const output)
{
  float value;
  const bool is_valid_float = TryParse(str, &value);
  if ((is_valid_float && value == 1) || !strcasecmp("true", str.c_str()))
    *output = true;
  else if ((is_valid_float && value == 0) || !strcasecmp("false", str.c_str()))
    *output = false;
  else
    return false;

  return true;
}

std::string ValueToString(u16 value)
{
  return fmt::format("0x{:04x}", value);
}

std::string ValueToString(u32 value)
{
  return fmt::format("0x{:08x}", value);
}

std::string ValueToString(u64 value)
{
  return fmt::format("0x{:016x}", value);
}

std::string ValueToString(float value)
{
  return fmt::format("{:#}", value);
}

std::string ValueToString(double value)
{
  return fmt::format("{:#}", value);
}

std::string ValueToString(int value)
{
  return std::to_string(value);
}

std::string ValueToString(s64 value)
{
  return std::to_string(value);
}

std::string ValueToString(bool value)
{
  return value ? "True" : "False";
}

bool SplitPath(std::string_view full_path, std::string* path, std::string* filename,
               std::string* extension)
{
  if (full_path.empty())
    return false;

  size_t dir_end = full_path.find_last_of("/"
// Windows needs the : included for something like just "C:" to be considered a directory
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

  if (path)
    *path = full_path.substr(0, dir_end);

  if (filename)
    *filename = full_path.substr(dir_end, fname_end - dir_end);

  if (extension)
    *extension = full_path.substr(fname_end);

  return true;
}

void UnifyPathSeparators(std::string& path)
{
#ifdef _WIN32
  for (char& c : path)
  {
    if (c == '\\')
      c = '/';
  }
#endif
}

std::string WithUnifiedPathSeparators(std::string path)
{
  UnifyPathSeparators(path);
  return path;
}

std::string PathToFileName(std::string_view path)
{
  std::string file_name, extension;
  SplitPath(path, nullptr, &file_name, &extension);
  return file_name + extension;
}

std::vector<std::string> SplitString(const std::string& str, const char delim)
{
  std::istringstream iss(str);
  std::vector<std::string> output(1);

  while (std::getline(iss, *output.rbegin(), delim))
    output.push_back("");

  output.pop_back();
  return output;
}

std::string TabsToSpaces(int tab_size, std::string str)
{
  const std::string spaces(tab_size, ' ');

  size_t i = 0;
  while (str.npos != (i = str.find('\t')))
    str.replace(i, 1, spaces);

  return str;
}

std::string ReplaceAll(std::string result, std::string_view src, std::string_view dest)
{
  size_t pos = 0;

  if (src == dest)
    return result;

  while ((pos = result.find(src, pos)) != std::string::npos)
  {
    result.replace(pos, src.size(), dest);
    pos += dest.length();
  }

  return result;
}

void StringPopBackIf(std::string* s, char c)
{
  if (!s->empty() && s->back() == c)
    s->pop_back();
}

// SLIPPITODO: look into boost locale maybe
void ConvertNarrowSpecialSHIFTJIS(std::string& input)
{
  // Melee doesn't correctly display special characters in narrow form We need to convert them to
  // wide form. I couldn't find a library to do this so for now let's just do it manually
  static std::unordered_map<char, char16_t> specialCharConvert = {
      {'!', (char16_t)0x8149},  {'"', (char16_t)0x8168}, {'#', (char16_t)0x8194},
      {'$', (char16_t)0x8190},  {'%', (char16_t)0x8193}, {'&', (char16_t)0x8195},
      {'\'', (char16_t)0x8166}, {'(', (char16_t)0x8169}, {')', (char16_t)0x816a},
      {'*', (char16_t)0x8196},  {'+', (char16_t)0x817b}, {',', (char16_t)0x8143},
      {'-', (char16_t)0x817c},  {'.', (char16_t)0x8144}, {'/', (char16_t)0x815e},
      {':', (char16_t)0x8146},  {';', (char16_t)0x8147}, {'<', (char16_t)0x8183},
      {'=', (char16_t)0x8181},  {'>', (char16_t)0x8184}, {'?', (char16_t)0x8148},
      {'@', (char16_t)0x8197},  {'[', (char16_t)0x816d}, {'\\', (char16_t)0x815f},
      {']', (char16_t)0x816e},  {'^', (char16_t)0x814f}, {'_', (char16_t)0x8151},
      {'`', (char16_t)0x814d},  {'{', (char16_t)0x816f}, {'|', (char16_t)0x8162},
      {'}', (char16_t)0x8170},  {'~', (char16_t)0x8160},
  };

  int pos = 0;
  while (pos < input.length())
  {
    auto c = input[pos];
    if ((u8)(0x80 & (u8)c) == 0x80)
    {
      // This is a 2 char rune, move to next
      pos += 2;
      continue;
    }

    bool hasConversion = specialCharConvert.count(c);
    if (!hasConversion)
    {
      pos += 1;
      continue;
    }

    // Remove previous character
    input.erase(pos, 1);

    // Add new chars to pos to replace
    auto newChars = (char*)&specialCharConvert[c];
    input.insert(input.begin() + pos, 1, newChars[0]);
    input.insert(input.begin() + pos, 1, newChars[1]);
  }
}

std::string TruncateLengthChar(const std::string& input, int length)
{
  auto utf32 = UTF8ToUTF32(input);

  // Limit length
  if (utf32.length() > length)
  {
    utf32.resize(length);
  }

  return UTF32toUTF8(utf32);
}

std::string ConvertStringForGame(const std::string& input, int length)
{
  auto utf8 = TruncateLengthChar(input, length);
  auto shiftJis = UTF8ToSHIFTJIS(utf8);
  ConvertNarrowSpecialSHIFTJIS(shiftJis);

  // Make fixed size
  shiftJis.resize(length * 2 + 1);
  return shiftJis;
}

size_t StringUTF8CodePointCount(std::string_view str)
{
  return str.size() - std::ranges::count_if(str, [](char c) -> bool { return (c & 0xC0) == 0x80; });
}

#ifdef _WIN32

static std::wstring CPToUTF16(u32 code_page, std::string_view input)
{
  auto const size =
      MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

  std::wstring output;
  output.resize(size);

  if (size == 0 ||
      size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                                  &output[0], static_cast<int>(output.size())))
  {
    output.clear();
  }

  return output;
}

static std::string UTF16ToCP(u32 code_page, std::wstring_view input)
{
  if (input.empty())
    return {};

  // "If cchWideChar [input buffer size] is set to 0, the function fails." -MSDN
  auto const size = WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()),
                                        nullptr, 0, nullptr, nullptr);

  std::string output(size, '\0');

  if (size != WideCharToMultiByte(code_page, 0, input.data(), static_cast<int>(input.size()),
                                  output.data(), static_cast<int>(output.size()), nullptr, nullptr))
  {
    const DWORD error_code = GetLastError();
    ERROR_LOG_FMT(COMMON, "WideCharToMultiByte Error in String '{}': {}", WStringToUTF8(input),
                  error_code);
    return {};
  }

  return output;
}

std::wstring UTF8ToWString(std::string_view input)
{
  return CPToUTF16(CP_UTF8, input);
}

std::string WStringToUTF8(std::wstring_view input)
{
  return UTF16ToCP(CP_UTF8, input);
}

std::string SHIFTJISToUTF8(std::string_view input)
{
  return WStringToUTF8(CPToUTF16(CODEPAGE_SHIFT_JIS, input));
}

std::string UTF8ToSHIFTJIS(std::string_view input)
{
  return UTF16ToCP(CODEPAGE_SHIFT_JIS, UTF8ToWString(input));
}

std::string CP1252ToUTF8(std::string_view input)
{
  return WStringToUTF8(CPToUTF16(CODEPAGE_WINDOWS_1252, input));
}

// SLIPPITODO: can't use string_view because still using deprecated codecvt stuff
// Unfortunately, there is no good replacement because all text encoding libraries suck.
// SLIPPITODO: look into MultiByteToWideChar() and WideCharToMultiByte() from <Windows.h>?
std::u32string UTF8ToUTF32(const std::string& input)
{
  std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> utf32Convert;
  auto asInt = utf32Convert.from_bytes(input);
  return std::u32string(reinterpret_cast<char32_t const*>(asInt.data()), asInt.length());
}

std::string UTF32toUTF8(const std::u32string& input)
{
  std::wstring_convert<std::codecvt_utf8<int32_t>, int32_t> utf8Convert;
  auto p = reinterpret_cast<const int32_t*>(input.data());
  return utf8Convert.to_bytes(p, p + input.size());
}

std::string UTF16BEToUTF8(const char16_t* str, size_t max_size)
{
  const char16_t* str_end = std::find(str, str + max_size, '\0');
  std::wstring result(static_cast<size_t>(str_end - str), '\0');
  std::transform(str, str_end, result.begin(), static_cast<u16 (&)(u16)>(Common::swap16));
  return WStringToUTF8(result);
}

#else

template <typename T>
#ifdef __APPLE__
std::string CodeToWithFallbacks(const char* tocode, const char* fromcode,
                                const std::basic_string_view<T> input, iconv_fallbacks* fallbacks)
#else
std::string CodeTo(const char* tocode, const char* fromcode, std::basic_string_view<T> input)
#endif
{
  std::string result;

  iconv_t const conv_desc = iconv_open(tocode, fromcode);

  // Only on OS X can we call iconvctl, it isn't found on Linux
#ifdef __APPLE__
  if (fallbacks)
  {
    iconvctl(conv_desc, ICONV_SET_FALLBACKS, fallbacks);
  }
#endif

  if ((iconv_t)-1 == conv_desc)
  {
    ERROR_LOG_FMT(COMMON, "Iconv initialization failure [{}]: {}", fromcode, strerror(errno));
  }
  else
  {
    size_t const in_bytes = sizeof(T) * input.size();
    size_t const out_buffer_size = 4 * in_bytes;

    std::string out_buffer;
    out_buffer.resize(out_buffer_size);

    auto src_buffer = input.data();
    size_t src_bytes = in_bytes;
    auto dst_buffer = out_buffer.data();
    size_t dst_bytes = out_buffer.size();

    while (src_bytes != 0)
    {
      size_t const iconv_result =
          iconv(conv_desc, const_cast<char**>(reinterpret_cast<const char**>(&src_buffer)),
                &src_bytes, &dst_buffer, &dst_bytes);
      if ((size_t)-1 == iconv_result)
      {
        if (EILSEQ == errno || EINVAL == errno)
        {
          // Try to skip the bad character
          if (src_bytes != 0)
          {
            --src_bytes;
            ++src_buffer;
          }
        }
        else
        {
          ERROR_LOG_FMT(COMMON, "iconv failure [{}]: {}", fromcode, strerror(errno));
          break;
        }
      }
    }

    out_buffer.resize(out_buffer_size - dst_bytes);
    out_buffer.swap(result);

    iconv_close(conv_desc);
  }

  return result;
}

#ifdef __APPLE__
template <typename T>
std::string CodeTo(const char* tocode, const char* fromcode, std::basic_string_view<T> input)
{
    return CodeToWithFallbacks(tocode, fromcode, input, nullptr);
}
#endif

template <typename T>
std::string CodeToUTF8(const char* fromcode, std::basic_string_view<T> input)
{
  return CodeTo("UTF-8", fromcode, input);
}

std::string CP1252ToUTF8(std::string_view input)
{
  // return CodeToUTF8("CP1252//TRANSLIT", input);
  // return CodeToUTF8("CP1252//IGNORE", input);
  return CodeToUTF8("CP1252", input);
}

std::string SHIFTJISToUTF8(std::string_view input)
{
  // return CodeToUTF8("CP932", input);
  return CodeToUTF8("SJIS", input);
}

#ifdef __APPLE__
void uc_to_mb_fb(unsigned int code,
                 void (*write_replacement)(const char* buf, size_t buflen, void* callback_arg),
                 void* callback_arg, void* data)
{
  static std::unordered_map<unsigned int, const char*> specialCharConvert = {
      {'!', (const char*)"\x81\x49"},
      {'"', (const char*)"\x81\x68"},
      {'#', (const char*)"\x81\x94"},
      {'$', (const char*)"\x81\x90"},
      {'%', (const char*)"\x81\x93"},
      {'&', (const char*)"\x81\x95"},
      {'\'', (const char*)"\x81\x66"},
      {'(', (const char*)"\x81\x69"},
      {')', (const char*)"\x81\x6a"},
      {'*', (const char*)"\x81\x96"},
      {'+', (const char*)"\x81\x7b"},
      {',', (const char*)"\x81\x43"},
      {'-', (const char*)"\x81\x7c"},
      {'.', (const char*)"\x81\x44"},
      {'/', (const char*)"\x81\x5e"},
      {':', (const char*)"\x81\x46"},
      {';', (const char*)"\x81\x47"},
      {'<', (const char*)"\x81\x83"},
      {'=', (const char*)"\x81\x81"},
      {'>', (const char*)"\x81\x84"},
      {'?', (const char*)"\x81\x48"},
      {'@', (const char*)"\x81\x97"},
      {'[', (const char*)"\x81\x6d"},
      {'\\', (const char*)"\x81\x5f"},
      {']', (const char*)"\x81\x6e"},
      {'^', (const char*)"\x81\x4f"},
      {'_', (const char*)"\x81\x51"},
      {'`', (const char*)"\x81\x4d"},
      {'{', (const char*)"\x81\x6f"},
      {'|', (const char*)"\x81\x62"},
      {'}', (const char*)"\x81\x70"},
      {'~', (const char*)"\x81\x60"},
      {U'¥', "\x81\x8f"},
      {U'•', "\x81\x45"},
      {U'—', "\x81\x7C"}};

  bool hasConversion = specialCharConvert.count(code);
  if (!hasConversion)
    return;

  auto newChar = specialCharConvert[code];
  // Add new chars to pos to replace
  write_replacement(newChar, 2, callback_arg);
}
#endif

std::string UTF8ToSHIFTJIS(std::string_view input)
{
#ifdef __APPLE__
  // Set SHIFTJIS callbacks only if converting to shift jis
  auto fallbacks = new iconv_fallbacks();
  fallbacks->uc_to_mb_fallback = uc_to_mb_fb;
  fallbacks->mb_to_uc_fallback = nullptr;
  fallbacks->mb_to_wc_fallback = nullptr;
  fallbacks->wc_to_mb_fallback = nullptr;
  fallbacks->data = nullptr;
  auto str = CodeToWithFallbacks("SJIS", "UTF-8", std::string_view(input), fallbacks);
  free(fallbacks);
#else
  auto str = CodeTo("SJIS", "UTF-8", std::string_view(input));
#endif
  return str;
}

std::string WStringToUTF8(std::wstring_view input)
{
  using codecvt = std::conditional_t<sizeof(wchar_t) == 2, std::codecvt_utf8_utf16<wchar_t>,
                                     std::codecvt_utf8<wchar_t>>;

  std::wstring_convert<codecvt, wchar_t> converter;
  return converter.to_bytes(input.data(), input.data() + input.size());
}

std::string UTF16BEToUTF8(const char16_t* str, size_t max_size)
{
  const char16_t* str_end = std::find(str, str + max_size, '\0');
  return CodeToUTF8("UTF-16BE", std::u16string_view(str, static_cast<size_t>(str_end - str)));
}

std::u32string UTF8ToUTF32(const std::string& input)
{
  auto val = CodeTo("UTF-32LE", "UTF-8", std::string_view(input));
  auto utf32Data = (char32_t*)val.data();
  return std::u32string(utf32Data, utf32Data + (val.size() / 4));
}

std::string UTF32toUTF8(const std::u32string& input)
{
  auto utf8Data = (char*)input.data();
  auto str = std::string(utf8Data, utf8Data + (input.size() * 4));
  return CodeTo("UTF-8", "UTF-32LE", std::string_view(str));
}

#endif

std::string UTF16ToUTF8(std::u16string_view input)
{
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.to_bytes(input.data(), input.data() + input.size());
}

std::u16string UTF8ToUTF16(std::string_view input)
{
  std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> converter;
  return converter.from_bytes(input.data(), input.data() + input.size());
}

// This is a replacement for path::u8path, which is deprecated starting with C++20.
std::filesystem::path StringToPath(std::string_view path)
{
#ifdef _MSC_VER
  return std::filesystem::path(UTF8ToWString(path));
#else
  return std::filesystem::path(path);
#endif
}

// This is a replacement for path::u8string that always has the return type std::string.
// path::u8string returns std::u8string starting with C++20, which is annoying to convert.
std::string PathToString(const std::filesystem::path& path)
{
#ifdef _MSC_VER
  return WStringToUTF8(path.native());
#else
  return path.native();
#endif
}

namespace Common
{
#ifdef _WIN32
std::vector<std::string> CommandLineToUtf8Argv(const wchar_t* command_line)
{
  int nargs;
  LPWSTR* tokenized = CommandLineToArgvW(command_line, &nargs);
  if (!tokenized)
    return {};

  std::vector<std::string> argv(nargs);
  for (size_t i = 0; i < nargs; ++i)
  {
    argv[i] = WStringToUTF8(tokenized[i]);
  }

  LocalFree(tokenized);
  return argv;
}
#endif

std::string GetEscapedHtml(std::string html)
{
  static constexpr std::array<std::array<const char*, 2>, 5> replacements{{
      // Escape ampersand first to avoid escaping the ampersands in other replacements
      {{"&", "&amp;"}},
      {{"<", "&lt;"}},
      {{">", "&gt;"}},
      {{"\"", "&quot;"}},
      {{"'", "&apos;"}},
  }};

  for (const auto& [unescaped, escaped] : replacements)
  {
    html = ReplaceAll(html, unescaped, escaped);
  }
  return html;
}

void ToLower(std::string* str)
{
  std::ranges::transform(*str, str->begin(), static_cast<char (&)(char)>(Common::ToLower));
}

void ToUpper(std::string* str)
{
  std::ranges::transform(*str, str->begin(), static_cast<char (&)(char)>(Common::ToUpper));
}

bool CaseInsensitiveEquals(std::string_view a, std::string_view b)
{
  return std::ranges::equal(
      a, b, [](char ca, char cb) { return Common::ToLower(ca) == Common::ToLower(cb); });
}

bool CaseInsensitiveLess::operator()(std::string_view a, std::string_view b) const
{
  return std::ranges::lexicographical_compare(
      a, b, [](char ca, char cb) { return Common::ToLower(ca) < Common::ToLower(cb); });
}

std::string BytesToHexString(std::span<const u8> bytes)
{
  return fmt::format("{:02x}", fmt::join(bytes, ""));
}
}  // namespace Common

