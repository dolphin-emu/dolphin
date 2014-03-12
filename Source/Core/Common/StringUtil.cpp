// Copyright 2013 Dolphin Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdarg>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <istream>
#include <limits.h>
#include <string>
#include <vector>

#include "Common/Common.h"
#include "Common/CommonPaths.h"
#include "Common/StringUtil.h"

#ifdef _WIN32
	#include <Windows.h>
#else
	#include <iconv.h>
	#include <errno.h>
#endif

// faster than sscanf
bool AsciiToHex(const std::string& _szValue, u32& result)
{
	char *endptr = nullptr;
	const u32 value = strtoul(_szValue.c_str(), &endptr, 16);

	if (!endptr || *endptr)
		return false;

	result = value;
	return true;
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
	// This is why we lookup an ANSI (cp1252) locale here and use _vsnprintf_l.
	static locale_t c_locale = nullptr;
	if (!c_locale)
		c_locale = _create_locale(LC_ALL, ".1252");
	writtenCount = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
	writtenCount = vsnprintf(out, outsize, format, args);
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
	char *buf = nullptr;
#ifdef _WIN32
	int required = 0;

	va_start(args, format);
	required = _vscprintf(format, args);
	buf = new char[required + 1];
	CharArrayFromFormatV(buf, required + 1, format, args);
	va_end(args);

	std::string temp = buf;
	delete[] buf;
#else
	va_start(args, format);
	if (vasprintf(&buf, format, args) < 0)
		ERROR_LOG(COMMON, "Unable to allocate memory for string");
	va_end(args);

	std::string temp = buf;
	free(buf);
#endif
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

bool TryParse(const std::string &str, u32 *const output)
{
	char *endptr = nullptr;

	// Reset errno to a value other than ERANGE
	errno = 0;

	unsigned long value = strtoul(str.c_str(), &endptr, 0);

	if (!endptr || *endptr)
		return false;

	if (errno == ERANGE)
		return false;

#if ULONG_MAX > UINT_MAX
	if (value >= 0x100000000ull &&
	    value <= 0xFFFFFFFF00000000ull)
		return false;
#endif

	*output = static_cast<u32>(value);
	return true;
}

bool TryParse(const std::string &str, bool *const output)
{
	if ("1" == str || !strcasecmp("true", str.c_str()))
		*output = true;
	else if ("0" == str || !strcasecmp("false", str.c_str()))
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

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest)
{
	while (1)
	{
		size_t pos = result.find(src);
		if (pos == std::string::npos) break;
		result.replace(pos, src.size(), dest);
	}
	return result;
}

// UriDecode and UriEncode are from http://www.codeguru.com/cpp/cpp/string/conversions/print.php/c12759
// by jinq0123 (November 2, 2006)

// Uri encode and decode.
// RFC1630, RFC1738, RFC2396

//#include <string>
//#include <assert.h>

const char HEX2DEC[256] =
{
	/*       0  1  2  3   4  5  6  7   8  9  A  B   C  D  E  F */
	/* 0 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* 1 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* 2 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* 3 */  0, 1, 2, 3,  4, 5, 6, 7,  8, 9,16,16, 16,16,16,16,

	/* 4 */ 16,10,11,12, 13,14,15,16, 16,16,16,16, 16,16,16,16,
	/* 5 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* 6 */ 16,10,11,12, 13,14,15,16, 16,16,16,16, 16,16,16,16,
	/* 7 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,

	/* 8 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* 9 */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* A */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* B */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,

	/* C */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* D */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* E */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16,
	/* F */ 16,16,16,16, 16,16,16,16, 16,16,16,16, 16,16,16,16
};

std::string UriDecode(const std::string & sSrc)
{
	// Note from RFC1630:  "Sequences which start with a percent sign
	// but are not followed by two hexadecimal characters (0-9, A-F) are reserved
	// for future extension"

	const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
	const size_t SRC_LEN = sSrc.length();
	const unsigned char * const SRC_END = pSrc + SRC_LEN;
	const unsigned char * const SRC_LAST_DEC = SRC_END - 2;   // last decodable '%'

	char * const pStart = new char[SRC_LEN];
	char * pEnd = pStart;

	while (pSrc < SRC_LAST_DEC)
	{
		if (*pSrc == '%')
		{
			char dec1, dec2;
			if (16 != (dec1 = HEX2DEC[*(pSrc + 1)]) &&
			    16 != (dec2 = HEX2DEC[*(pSrc + 2)]))
			{
				*pEnd++ = (dec1 << 4) + dec2;
				pSrc += 3;
				continue;
			}
		}

		*pEnd++ = *pSrc++;
	}

	// the last 2- chars
	while (pSrc < SRC_END)
		*pEnd++ = *pSrc++;

	std::string sResult(pStart, pEnd);
	delete [] pStart;
	return sResult;
}

// Only alphanum is safe.
const char SAFE[256] =
{
	/*      0 1 2 3  4 5 6 7  8 9 A B  C D E F */
	/* 0 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* 1 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* 2 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* 3 */ 1,1,1,1, 1,1,1,1, 1,1,0,0, 0,0,0,0,

	/* 4 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
	/* 5 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,
	/* 6 */ 0,1,1,1, 1,1,1,1, 1,1,1,1, 1,1,1,1,
	/* 7 */ 1,1,1,1, 1,1,1,1, 1,1,1,0, 0,0,0,0,

	/* 8 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* 9 */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* A */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* B */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,

	/* C */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* D */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* E */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0,
	/* F */ 0,0,0,0, 0,0,0,0, 0,0,0,0, 0,0,0,0
};

std::string UriEncode(const std::string & sSrc)
{
	const char DEC2HEX[16 + 1] = "0123456789ABCDEF";
	const unsigned char * pSrc = (const unsigned char *)sSrc.c_str();
	const size_t SRC_LEN = sSrc.length();
	unsigned char * const pStart = new unsigned char[SRC_LEN * 3];
	unsigned char * pEnd = pStart;
	const unsigned char * const SRC_END = pSrc + SRC_LEN;

	for (; pSrc < SRC_END; ++pSrc)
	{
		if (SAFE[*pSrc])
			*pEnd++ = *pSrc;
		else
		{
			// escape this char
			*pEnd++ = '%';
			*pEnd++ = DEC2HEX[*pSrc >> 4];
			*pEnd++ = DEC2HEX[*pSrc & 0x0F];
		}
	}

	std::string sResult((char *)pStart, (char *)pEnd);
	delete [] pStart;
	return sResult;
}

#ifdef _WIN32

std::string UTF16ToUTF8(const std::wstring& input)
{
	auto const size = WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), nullptr, 0, nullptr, nullptr);

	std::string output;
	output.resize(size);

	if (size == 0 || size != WideCharToMultiByte(CP_UTF8, 0, input.data(), (int)input.size(), &output[0], (int)output.size(), nullptr, nullptr))
	{
		output.clear();
	}

	return output;
}

std::wstring CPToUTF16(u32 code_page, const std::string& input)
{
	auto const size = MultiByteToWideChar(code_page, 0, input.data(), (int)input.size(), nullptr, 0);

	std::wstring output;
	output.resize(size);

	if (size == 0 || size != MultiByteToWideChar(code_page, 0, input.data(), (int)input.size(), &output[0], (int)output.size()))
	{
		output.clear();
	}

	return output;
}

std::wstring UTF8ToUTF16(const std::string& input)
{
	return CPToUTF16(CP_UTF8, input);
}

std::string SHIFTJISToUTF8(const std::string& input)
{
	return UTF16ToUTF8(CPToUTF16(932, input));
}

std::string CP1252ToUTF8(const std::string& input)
{
	return UTF16ToUTF8(CPToUTF16(1252, input));
}

#else

template <typename T>
std::string CodeToUTF8(const char* fromcode, const std::basic_string<T>& input)
{
	std::string result;

	iconv_t const conv_desc = iconv_open("UTF-8", fromcode);
	if ((iconv_t)-1 == conv_desc)
	{
		ERROR_LOG(COMMON, "Iconv initialization failure [%s]: %s", fromcode, strerror(errno));
	}
	else
	{
		size_t const in_bytes = sizeof(T) * input.size();
		size_t const out_buffer_size = 4 * in_bytes;

		std::string out_buffer;
		out_buffer.resize(out_buffer_size);

		auto src_buffer = &input[0];
		size_t src_bytes = in_bytes;
		auto dst_buffer = &out_buffer[0];
		size_t dst_bytes = out_buffer.size();

		while (src_bytes != 0)
		{
			size_t const iconv_result = iconv(conv_desc, (char**)(&src_buffer), &src_bytes,
				&dst_buffer, &dst_bytes);

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
					ERROR_LOG(COMMON, "iconv failure [%s]: %s", fromcode, strerror(errno));
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

std::string CP1252ToUTF8(const std::string& input)
{
	//return CodeToUTF8("CP1252//TRANSLIT", input);
	//return CodeToUTF8("CP1252//IGNORE", input);
	return CodeToUTF8("CP1252", input);
}

std::string SHIFTJISToUTF8(const std::string& input)
{
	//return CodeToUTF8("CP932", input);
	return CodeToUTF8("SJIS", input);
}

std::string UTF16ToUTF8(const std::wstring& input)
{
	std::string result = CodeToUTF8("UTF-16LE", input);

	// TODO: why is this needed?
	result.erase(std::remove(result.begin(), result.end(), 0x00), result.end());
	return result;
}

#endif
