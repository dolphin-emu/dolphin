// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/StringUtil.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"

#ifdef _WIN32
#include <Windows.h>
#include <shellapi.h>
constexpr u32 CODEPAGE_SHIFT_JIS = 932;
constexpr u32 CODEPAGE_WINDOWS_1252 = 1252;

#include "Common/Swap.h"
#else
#include <cerrno>
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
  const int written_count = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  locale_t previousLocale = uselocale(GetCLocale());
#endif
  const int written_count = vsnprintf(out, outsize, format, args);
#if !defined(ANDROID) && !defined(__HAIKU__) && !defined(__OpenBSD__) && !defined(__NetBSD__)
  uselocale(previousLocale);
#endif
#endif

  if (written_count > 0 && written_count < outsize)
  {
    out[written_count] = '\0';
    return true;
  }

  out[outsize - 1] = '\0';
  return false;
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

  for (int line = 0; size != 0; ++data, --size)
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

  size_t dir_end = full_path.find_last_of(
// Windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
      "/:"
#else
      '/'
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
  std::string file_name;
  std::string extension;
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

size_t StringUTF8CodePointCount(std::string_view str)
{
  return str.size() - std::ranges::count_if(str, [](char c) -> bool { return (c & 0xC0) == 0x80; });
}

constexpr char32_t UNICODE_REPLACEMENT_CHARACTER = 0xfffd;
constexpr char32_t UNICODE_LAST_CODE_POINT = 0x10ffff;

constexpr u16 UNICODE_HIGH_SURROGATE = 0xd800;
constexpr u16 UNICODE_LOW_SURROGATE = 0xdc00;

constexpr u16 SURROGATE_VALUE_MASK = 0x3ffu;

static constexpr bool IsSurrogateCodePoint(char32_t code_point)
{
  return (code_point & 0xf800u) == UNICODE_HIGH_SURROGATE;
}

template <std::integral CharType>
requires(sizeof(CharType) == 1)
class UTF8Decoder
{
public:
  constexpr explicit UTF8Decoder(std::span<const CharType> chars)
      : m_ptr{chars.data()}, m_end_ptr{m_ptr + chars.size()}
  {
  }

  auto RemainingCodeUnits() const { return m_end_ptr - m_ptr; }

  constexpr char32_t operator()()
  {
    assert(RemainingCodeUnits() > 0);

    const u8 first_code_unit = *m_ptr;
    ++m_ptr;

    switch (std::countl_one(first_code_unit))
    {
    case 0:  // ASCII.
      return first_code_unit;
    case 2:
      return FinishReadingSequence<2, 0x80>(first_code_unit);
    case 3:
    {
      const u32 code_point = FinishReadingSequence<3, 0x800>(first_code_unit);
      if (!IsSurrogateCodePoint(code_point))
        return code_point;
      break;
    }
    case 4:
    {
      const u32 code_point = FinishReadingSequence<4, 0x10000>(first_code_unit);
      if (code_point <= UNICODE_LAST_CODE_POINT)
        return code_point;
      break;
    }
    default:
      break;
    }

    return UNICODE_REPLACEMENT_CHARACTER;
  }

private:
  template <u32 ByteCount, u32 FirstValidCodePoint>
  constexpr u32 FinishReadingSequence(u8 first_code_unit)
  {
    // Remove the leading one bits.
    u32 code_point = first_code_unit & (0x7fu >> ByteCount);

    for (u32 byte_count = ByteCount - 1; byte_count != 0; --byte_count)
    {
      if (RemainingCodeUnits() == 0)
        return UNICODE_REPLACEMENT_CHARACTER;

      const auto code_unit = u8(*m_ptr);

      if (!IsContinuationByte(code_unit))
        return UNICODE_REPLACEMENT_CHARACTER;

      ++m_ptr;

      code_point = (code_point << 6u) | (code_unit & 0x3fu);
    }

    // Overlong encoding.
    if (code_point < FirstValidCodePoint)
      return UNICODE_REPLACEMENT_CHARACTER;

    return code_point;
  }

  static constexpr bool IsContinuationByte(u8 code_unit) { return std::countl_one(code_unit) == 1; }

  const CharType* m_ptr;
  const CharType* const m_end_ptr;
};

class UTF8Encoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 4; }

  // `ptr` should point to at least 4 bytes.
  // Returns the number of written code units (bytes).
  template <std::integral CharType>
  requires(sizeof(CharType) == 1)
  constexpr u32 operator()(char32_t code_point, CharType* ptr)
  {
    // ASCII.
    if (code_point < 0x80u)
    {
      *ptr = u8(code_point);
      return 1;
    }

    if (code_point < 0x800u)
      return WriteSequence<2>(code_point, ptr);

    if (code_point < 0x10000u)
      return WriteSequence<3>(code_point, ptr);

    if (code_point <= UNICODE_LAST_CODE_POINT)
      return WriteSequence<4>(code_point, ptr);

    return (*this)(UNICODE_REPLACEMENT_CHARACTER, ptr);
  }

private:
  template <u32 ByteCount>
  static constexpr u32 WriteSequence(u32 code_point, auto* ptr)
  {
    *ptr = u8((0xf0u << (4 - ByteCount)) | (code_point >> (6 * (ByteCount - 1))));

    for (u32 i = ByteCount - 1; i != 0; --i)
    {
      ptr[i] = u8(0x80u | (code_point & 0x3fu));
      code_point >>= 6;
    }

    return ByteCount;
  }
};

template <std::integral CharType>
requires(sizeof(CharType) == 2)
class UTF16Decoder
{
public:
  constexpr explicit UTF16Decoder(std::span<const CharType> chars)
      : m_ptr{chars.data()}, m_end_ptr{m_ptr + chars.size()}
  {
  }

  auto RemainingCodeUnits() const { return m_end_ptr - m_ptr; }

  constexpr char32_t operator()()
  {
    assert(RemainingCodeUnits() > 0);

    const u16 first_code_unit = *m_ptr;
    ++m_ptr;

    // Single code unit.
    if (!IsSurrogateCodePoint(first_code_unit))
      return first_code_unit;

    // Unexpected low surrogate.
    if (first_code_unit >= UNICODE_LOW_SURROGATE)
      return UNICODE_REPLACEMENT_CHARACTER;

    // High surrogate at end of data.
    if (RemainingCodeUnits() == 0)
      return UNICODE_REPLACEMENT_CHARACTER;

    const u16 second_code_unit = *m_ptr;

    // High surrogate not followed by low surrogate.
    if ((second_code_unit & u16(~SURROGATE_VALUE_MASK)) != UNICODE_LOW_SURROGATE)
      return UNICODE_REPLACEMENT_CHARACTER;

    ++m_ptr;

    // We have a surrogate pair.
    return (u32(first_code_unit & SURROGATE_VALUE_MASK) << 10u) +
           u32(second_code_unit & SURROGATE_VALUE_MASK) + 0x10000u;
  }

private:
  const CharType* m_ptr;
  const CharType* const m_end_ptr;
};

class UTF16Encoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 2; }

  // `ptr` should point to at least 2 code units.
  // Returns the number of written code units.
  template <std::integral CharType>
  requires(sizeof(CharType) == 2)
  constexpr u32 operator()(char32_t code_point, CharType* ptr)
  {
    if (code_point < 0x10000u)
    {
      *ptr = u16(code_point);
      return 1;
    }

    if (code_point > UNICODE_LAST_CODE_POINT)
      return (*this)(UNICODE_REPLACEMENT_CHARACTER, ptr);

    // Create surrogate pair.
    const u32 value = code_point - 0x10000;
    ptr[0] = u16(((value >> 10u) & SURROGATE_VALUE_MASK) | UNICODE_HIGH_SURROGATE);
    ptr[1] = u16((value & SURROGATE_VALUE_MASK) | UNICODE_LOW_SURROGATE);
    return 2;
  }
};

template <typename Decoder, typename Encoder, typename ResultCharType, typename InputCharType>
static constexpr std::basic_string<ResultCharType>
ReEncodeString(std::basic_string_view<InputCharType> input)
{
  Decoder decoder{input};
  Encoder encoder;

  const auto max_code_units = input.size() * encoder.GetMaxUnitsPerCodePoint();

  std::basic_string<ResultCharType> result;
  result.resize_and_overwrite(max_code_units, [&](ResultCharType* buf, std::size_t) {
    auto* position = buf;

    while (decoder.RemainingCodeUnits() != 0)
      position += encoder(decoder(), position);

    return position - buf;
  });

  return result;
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

std::string UTF16BEToUTF8(const char16_t* str, size_t max_size)
{
  const char16_t* str_end = std::find(str, str + max_size, '\0');
  std::wstring result(static_cast<size_t>(str_end - str), '\0');
  std::transform(str, str_end, result.begin(), static_cast<u16 (&)(u16)>(Common::swap16));
  return WStringToUTF8(result);
}

#else

template <typename T>
static std::string CodeTo(const char* tocode, const char* fromcode, std::basic_string_view<T> input)
{
  std::string result;

  auto* const conv_desc = iconv_open(tocode, fromcode);
  if ((iconv_t)-1 == conv_desc)
  {
    ERROR_LOG_FMT(COMMON, "Iconv initialization failure [{}]: {}", fromcode,
                  Common::LastStrerrorString());
  }
  else
  {
    size_t const in_bytes = sizeof(T) * input.size();
    size_t const out_buffer_size = 4 * in_bytes;

    std::string out_buffer;
    out_buffer.resize(out_buffer_size);

    auto* src_buffer = input.data();
    size_t src_bytes = in_bytes;
    auto* dst_buffer = out_buffer.data();
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
          ERROR_LOG_FMT(COMMON, "iconv failure [{}]: {}", fromcode, Common::LastStrerrorString());
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

template <typename T>
static std::string CodeToUTF8(const char* fromcode, std::basic_string_view<T> input)
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

std::string UTF8ToSHIFTJIS(std::string_view input)
{
  return CodeTo("SJIS", "UTF-8", input);
}

std::string WStringToUTF8(std::wstring_view input)
{
  // Note: Without LE iconv expects a BOM.
  // The "WCHAR_T" code would be appropriate, but it's apparently not in every iconv implementation.
  return CodeToUTF8((sizeof(wchar_t) == 2) ? "UTF-16LE" : "UTF-32LE", input);
}

std::string UTF16BEToUTF8(const char16_t* str, size_t max_size)
{
  const char16_t* str_end = std::find(str, str + max_size, '\0');
  return CodeToUTF8("UTF-16BE", std::u16string_view(str, static_cast<size_t>(str_end - str)));
}

#endif

std::string UTF16ToUTF8(std::u16string_view input)
{
  return ReEncodeString<UTF16Decoder<char16_t>, UTF8Encoder, char>(input);
}

std::u16string UTF8ToUTF16(std::string_view input)
{
  return ReEncodeString<UTF8Decoder<char>, UTF16Encoder, char16_t>(input);
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

bool CaseInsensitiveContains(std::string_view haystack, std::string_view needle)
{
  if (needle.empty())
    return true;

  for (size_t i = 0; i + needle.size() <= haystack.size(); ++i)
  {
    if (std::ranges::equal(needle, haystack.substr(i, needle.size()),
                           [](char a, char b) { return Common::ToLower(a) == Common::ToLower(b); }))
    {
      return true;
    }
  }
  return false;
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
