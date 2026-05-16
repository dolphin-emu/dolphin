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
#include <cwchar>
#include <iomanip>
#include <iterator>
#include <sstream>
#include <string>
#include <vector>

#include <fmt/format.h>
#include <fmt/ranges.h>

#if defined(_WIN32)
#include <shellapi.h>

#include "Common/CommonFuncs.h"
#endif

#include "Common/CommonTypes.h"
#include "Common/Logging/Log.h"
#include "Common/ShiftJIS.h"

template <typename T, int ByteSize>
concept SizedIntegral = std::integral<T> && sizeof(T) == ByteSize;

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

constexpr u16 UNICODE_HIGH_SURROGATE = 0xd800;
constexpr u16 UNICODE_LOW_SURROGATE = 0xdc00;

constexpr u16 SURROGATE_VALUE_MASK = 0x3ffu;

static constexpr bool IsSurrogateCodePoint(char32_t code_point)
{
  return (code_point & 0xf800u) == UNICODE_HIGH_SURROGATE;
}

template <typename CharType, auto TransformFunction = std::identity{}>
class CodeUnitReader
{
public:
  constexpr explicit CodeUnitReader(std::span<const CharType> chars)
      : m_ptr{chars.data()}, m_end_ptr{m_ptr + chars.size()}
  {
  }

  constexpr auto RemainingCodeUnits() const { return m_end_ptr - m_ptr; }

protected:
  constexpr auto ReadCodeUnit()
  {
    const auto result = PeekCodeUnit();
    AdvanceReader();
    return result;
  }

  constexpr auto PeekCodeUnit() const
  {
    assert(RemainingCodeUnits() > 0);
    return TransformFunction(*m_ptr);
  }

  constexpr void AdvanceReader() { ++m_ptr; }

private:
  const CharType* m_ptr;
  const CharType* const m_end_ptr;
};

template <SizedIntegral<4> CharType>
class UTF32Decoder : public CodeUnitReader<CharType>
{
public:
  using CodeUnitReader<CharType>::CodeUnitReader;

  constexpr char32_t operator()() { return this->ReadCodeUnit(); }
};

class UTF32Encoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 1; }

  constexpr u32 operator()(char32_t code_point, SizedIntegral<4> auto* ptr)
  {
    *ptr = code_point;
    return 1;
  }
};

template <SizedIntegral<1> CharType>
class CP1252Decoder : public CodeUnitReader<CharType>
{
public:
  using CodeUnitReader<CharType>::CodeUnitReader;

  constexpr char32_t operator()()
  {
    const u8 code_unit = this->ReadCodeUnit();

    // ISO/IEC 8859-1 "extended ASCII" equivalent values.
    if (code_unit < 0x80u || code_unit > 0x9fu)
      return code_unit;

    static constexpr auto unused = UNICODE_REPLACEMENT_CHARACTER;

    static constexpr std::array<u16, 0x20> values_from_80_to_9f = {
        0x20ac, unused, 0x201a, 0x0192, 0x201e, 0x2026, 0x2020, 0x2021, 0x02c6, 0x2030, 0x0160,
        0x2039, 0x0152, unused, 0x017d, unused, unused, 0x2018, 0x2019, 0x201c, 0x201d, 0x2022,
        0x2013, 0x2014, 0x02dc, 0x2122, 0x0161, 0x203a, 0x0153, unused, 0x017e, 0x0178};

    return values_from_80_to_9f[code_unit - 0x80u];
  }
};

template <SizedIntegral<1> CharType>
class ShiftJISDecoder : public CodeUnitReader<CharType>
{
public:
  using CodeUnitReader<CharType>::CodeUnitReader;

  constexpr char32_t operator()()
  {
    const u8 first_code_unit = this->ReadCodeUnit();
    char32_t code_point = Common::DecodeSingleByteShiftJIS(first_code_unit);

    if (code_point != UNICODE_REPLACEMENT_CHARACTER)
      return code_point;

    if (this->RemainingCodeUnits() == 0)
      return UNICODE_REPLACEMENT_CHARACTER;

    code_point = Common::DecodeDoubleByteShiftJIS(first_code_unit, this->PeekCodeUnit());

    if (code_point != UNICODE_REPLACEMENT_CHARACTER)
      this->AdvanceReader();

    return code_point;
  }
};

class ShiftJISEncoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 2; }

  // `ptr` should point to at least 2 bytes.
  // Returns the number of written code units (bytes).
  constexpr u32 operator()(char32_t code_point, SizedIntegral<1> auto* ptr)
  {
    const u16 shift_jis_code = Common::EncodeShiftJIS(code_point);

    if (shift_jis_code < 0x100u)
    {
      *ptr = u8(shift_jis_code);
      return 1;
    }

    // Encode a '?' symbol for unsupported code points.
    // FYI: Shift JIS does not support Unicode "Specials".
    if (shift_jis_code == u16(-1))
      return (*this)('?', ptr);

    ptr[0] = u8(shift_jis_code >> 8u);
    ptr[1] = u8(shift_jis_code);
    return 2;
  }
};

template <SizedIntegral<1> CharType>
class UTF8Decoder : public CodeUnitReader<CharType>
{
public:
  using CodeUnitReader<CharType>::CodeUnitReader;

  constexpr char32_t operator()()
  {
    const u8 first_code_unit = this->ReadCodeUnit();

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
      if (this->RemainingCodeUnits() == 0)
        return UNICODE_REPLACEMENT_CHARACTER;

      const u8 code_unit = this->PeekCodeUnit();

      if (!IsContinuationByte(code_unit))
        return UNICODE_REPLACEMENT_CHARACTER;

      this->AdvanceReader();

      code_point = (code_point << 6u) | (code_unit & 0x3fu);
    }

    // Overlong encoding.
    if (code_point < FirstValidCodePoint)
      return UNICODE_REPLACEMENT_CHARACTER;

    return code_point;
  }

  static constexpr bool IsContinuationByte(u8 code_unit) { return std::countl_one(code_unit) == 1; }
};

class UTF8Encoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 4; }

  // `ptr` should point to at least 4 bytes.
  // Returns the number of written code units (bytes).
  constexpr u32 operator()(char32_t code_point, SizedIntegral<1> auto* ptr)
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

template <SizedIntegral<2> CharType, auto TransformFunction = std::identity{}>
class UTF16Decoder : public CodeUnitReader<CharType, TransformFunction>
{
public:
  using CodeUnitReader<CharType, TransformFunction>::CodeUnitReader;

  constexpr char32_t operator()()
  {
    const u16 first_code_unit = this->ReadCodeUnit();

    // Single code unit.
    if (!IsSurrogateCodePoint(first_code_unit))
      return first_code_unit;

    // Unexpected low surrogate.
    if (first_code_unit >= UNICODE_LOW_SURROGATE)
      return UNICODE_REPLACEMENT_CHARACTER;

    // High surrogate at end of data.
    if (this->RemainingCodeUnits() == 0)
      return UNICODE_REPLACEMENT_CHARACTER;

    const u16 second_code_unit = this->PeekCodeUnit();

    // High surrogate not followed by low surrogate.
    if ((second_code_unit & u16(~SURROGATE_VALUE_MASK)) != UNICODE_LOW_SURROGATE)
      return UNICODE_REPLACEMENT_CHARACTER;

    this->AdvanceReader();

    // We have a surrogate pair.
    return (u32(first_code_unit & SURROGATE_VALUE_MASK) << 10u) +
           u32(second_code_unit & SURROGATE_VALUE_MASK) + 0x10000u;
  }
};

template <typename CharType>
using UTF16BEDecoder = UTF16Decoder<CharType, std::byteswap<CharType>>;

class UTF16Encoder
{
public:
  static constexpr u32 GetMaxUnitsPerCodePoint() { return 2; }

  // `ptr` should point to at least 2 code units.
  // Returns the number of written code units.
  constexpr u32 operator()(char32_t code_point, SizedIntegral<2> auto* ptr)
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

#if WCHAR_MAX == 0xffff || WCHAR_MAX == 0x7fff
using WCharDecoder = UTF16Decoder<wchar_t>;
using WCharEncoder = UTF16Encoder;
#else
using WCharDecoder = UTF32Decoder<wchar_t>;
using WCharEncoder = UTF32Encoder;
#endif

template <typename Decoder, typename Encoder, typename ResultCharType, typename InputCharType>
static constexpr std::basic_string<ResultCharType>
ReEncodeString(std::basic_string_view<InputCharType> input)
{
  Encoder encoder;

  const auto buffer_size = input.size() * encoder.GetMaxUnitsPerCodePoint();

  // Handle the unlikely case of integer overflow in the buffer size.
  const auto input_size = buffer_size / encoder.GetMaxUnitsPerCodePoint();

  Decoder decoder{std::span{input}.first(input_size)};

  std::basic_string<ResultCharType> result;
  result.resize_and_overwrite(buffer_size, [&](ResultCharType* buf, std::size_t) {
    auto* position = buf;

    while (decoder.RemainingCodeUnits() != 0)
      position += encoder(decoder(), position);

    return position - buf;
  });

  return result;
}

std::string CP1252ToUTF8(std::string_view input)
{
  return ReEncodeString<CP1252Decoder<char>, UTF8Encoder, char>(input);
}

std::string SHIFTJISToUTF8(std::string_view input)
{
  return ReEncodeString<ShiftJISDecoder<char>, UTF8Encoder, char>(input);
}

std::string UTF8ToSHIFTJIS(std::string_view input)
{
  return ReEncodeString<UTF8Decoder<char>, ShiftJISEncoder, char>(input);
}

std::string WStringToUTF8(std::wstring_view input)
{
  return ReEncodeString<WCharDecoder, UTF8Encoder, char>(input);
}

std::wstring UTF8ToWString(std::string_view input)
{
  return ReEncodeString<UTF8Decoder<char>, WCharEncoder, wchar_t>(input);
}

std::string UTF16BEToUTF8(const char16_t* str, size_t max_size)
{
  const char16_t* str_end = std::find(str, str + max_size, '\0');
  return ReEncodeString<UTF16BEDecoder<char16_t>, UTF8Encoder, char>(
      std::basic_string_view{str, str_end});
}

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
