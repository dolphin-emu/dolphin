// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include <gtest/gtest.h>

#include <string>
#include <vector>

#include "Common/Network.h"
#include "Common/StringUtil.h"
#include "Common/Swap.h"

TEST(StringUtil, StringPopBackIf)
{
  std::string abc = "abc";
  std::string empty;

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STREQ("abc", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'c');
  StringPopBackIf(&empty, 'c');
  EXPECT_STRNE("abc", abc.c_str());
  EXPECT_STREQ("ab", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'b');
  StringPopBackIf(&empty, 'b');
  EXPECT_STRNE("ab", abc.c_str());
  EXPECT_STREQ("a", abc.c_str());
  EXPECT_STRNE(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STRNE("a", abc.c_str());
  EXPECT_STREQ("", abc.c_str());
  EXPECT_STREQ(empty.c_str(), abc.c_str());

  StringPopBackIf(&abc, 'a');
  StringPopBackIf(&empty, 'a');
  EXPECT_STREQ("", abc.c_str());
  EXPECT_STREQ(empty.c_str(), abc.c_str());
}

TEST(StringUtil, UTF8ToSHIFTJIS)
{
  const std::string kirby_unicode =
      "\xe6\x98\x9f\xe3\x81\xae\xe3\x82\xab\xe3\x83\xbc\xe3\x83\x93\xe3\x82\xa3";
  const std::string kirby_sjis = "\x90\xaf\x82\xcc\x83\x4a\x81\x5b\x83\x72\x83\x42";

  EXPECT_STREQ(SHIFTJISToUTF8(UTF8ToSHIFTJIS(kirby_unicode)).c_str(), kirby_unicode.c_str());
  EXPECT_STREQ(UTF8ToSHIFTJIS(kirby_unicode).c_str(), kirby_sjis.c_str());
}

template <typename T>
static void DoRoundTripTest(const std::vector<T>& data)
{
  for (const T& e : data)
  {
    const std::string s = ValueToString(e);
    T out = T();
    EXPECT_TRUE(TryParse(s, &out));
    EXPECT_EQ(e, out);
  }
}

TEST(StringUtil, ToString_TryParse_Roundtrip)
{
  DoRoundTripTest<bool>({true, false});
  DoRoundTripTest<int>({0, -1, 1, 123, -123, 123456789, -123456789});
  DoRoundTripTest<unsigned int>({0u, 1u, 123u, 123456789u, 4023456789u});
  DoRoundTripTest<float>({0.0f, 1.0f, -1.0f, -0.5f, 0.5f, -1e-3f, 1e-3f, 1e3f, -1e3f});
  DoRoundTripTest<double>({0.0, 1.0, -1.0, -0.5, 0.5, -1e-3, 1e-3, 1e3, -1e3});
}

TEST(StringUtil, GetEscapedHtml)
{
  static constexpr auto no_escape_needed =
      "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
      "!@#$%^*()-_=+,./?;:[]{}| \\\t\n";
  EXPECT_EQ(Common::GetEscapedHtml(no_escape_needed), no_escape_needed);
  EXPECT_EQ(Common::GetEscapedHtml("&<>'\""), "&amp;&lt;&gt;&apos;&quot;");
  EXPECT_EQ(Common::GetEscapedHtml("&&&"), "&amp;&amp;&amp;");
}

TEST(StringUtil, SplitPath)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/lib/some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "/usr/lib/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathNullOutputPathAllowed)
{
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/lib/some_file.txt", /*path=*/nullptr, &filename, &extension));
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathNullOutputFilenameAllowed)
{
  std::string path;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/lib/some_file.txt", &path, /*filename=*/nullptr, &extension));
  EXPECT_EQ(path, "/usr/lib/");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathNullOutputExtensionAllowed)
{
  std::string path;
  std::string filename;
  EXPECT_TRUE(SplitPath("/usr/lib/some_file.txt", &path, &filename, /*extension=*/nullptr));
  EXPECT_EQ(path, "/usr/lib/");
  EXPECT_EQ(filename, "some_file");
}

TEST(StringUtil, SplitPathReturnsFalseIfFullPathIsEmpty)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_FALSE(SplitPath(/*full_path=*/"", &path, &filename, &extension));
  EXPECT_EQ(path, "");
  EXPECT_EQ(filename, "");
  EXPECT_EQ(extension, "");
}

TEST(StringUtil, SplitPathNoPath)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathNoFileName)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/lib/.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "/usr/lib/");
  EXPECT_EQ(filename, "");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathNoExtension)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/lib/some_file", &path, &filename, &extension));
  EXPECT_EQ(path, "/usr/lib/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, "");
}

TEST(StringUtil, SplitPathDifferentPathLengths)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("/usr/some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "/usr/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");

  EXPECT_TRUE(SplitPath("/usr/lib/foo/some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "/usr/lib/foo/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");
}

TEST(StringUtil, SplitPathBackslashesNotRecognizedAsSeparators)
{
  std::string path;
  std::string filename;
  std::string extension;
  EXPECT_TRUE(SplitPath("\\usr\\some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "");
  EXPECT_EQ(filename, "\\usr\\some_file");
  EXPECT_EQ(extension, ".txt");
}

#ifdef _WIN32
TEST(StringUtil, SplitPathWindowsPathWithDriveLetter)
{
  // Verify that on Windows, valid paths that include a drive letter and volume separator (e.g.,
  // "C:") parse correctly.
  std::string path;
  std::string filename;
  std::string extension;

  // Absolute path with drive letter
  EXPECT_TRUE(SplitPath("C:/dir/some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "C:/dir/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");

  // Relative path with drive letter
  EXPECT_TRUE(SplitPath("C:dir/some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "C:dir/");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");

  // Relative path with drive letter and no directory
  EXPECT_TRUE(SplitPath("C:some_file.txt", &path, &filename, &extension));
  EXPECT_EQ(path, "C:");
  EXPECT_EQ(filename, "some_file");
  EXPECT_EQ(extension, ".txt");

  // Path that is just the drive letter
  EXPECT_TRUE(SplitPath("C:", &path, &filename, &extension));
  EXPECT_EQ(path, "C:");
  EXPECT_EQ(filename, "");
  EXPECT_EQ(extension, "");
}
#endif

TEST(StringUtil, CaseInsensitiveContains_BasicMatches)
{
  EXPECT_TRUE(Common::CaseInsensitiveContains("hello world", "hello"));
  EXPECT_TRUE(Common::CaseInsensitiveContains("hello world", "world"));
  EXPECT_TRUE(Common::CaseInsensitiveContains("HELLO WORLD", "hello"));
  EXPECT_TRUE(Common::CaseInsensitiveContains("HeLLo WoRLd", "WORLD"));
}

TEST(StringUtil, CaseInsensitiveContains_SubstringNotFound)
{
  EXPECT_FALSE(Common::CaseInsensitiveContains("hello world", "hey"));
}

TEST(StringUtil, CaseInsensitiveContains_EmptyStrings)
{
  EXPECT_TRUE(Common::CaseInsensitiveContains("", ""));
  EXPECT_TRUE(Common::CaseInsensitiveContains("hello", ""));
  EXPECT_FALSE(Common::CaseInsensitiveContains("", "world"));
}

TEST(StringUtil, CaseInsensitiveContains_EntireStringMatch)
{
  EXPECT_TRUE(Common::CaseInsensitiveContains("Test", "TEST"));
}

TEST(StringUtil, CaseInsensitiveContains_OverlappingMatches)
{
  EXPECT_TRUE(Common::CaseInsensitiveContains("aaaaaa", "aa"));
  EXPECT_TRUE(Common::CaseInsensitiveContains("ababababa", "bABa"));
}

TEST(StringUtil, SplitStringIntoArray)
{
  constexpr auto subject0 = "";
  EXPECT_FALSE(SplitStringIntoArray<2>(subject0, '.').has_value());

  constexpr auto subject1 = "hello";
  EXPECT_FALSE(SplitStringIntoArray<2>(subject1, '.').has_value());

  constexpr auto subject2 = "hello.world";
  const auto split2 = SplitStringIntoArray<2>(subject2, '.');
  EXPECT_TRUE(split2.has_value() && split2->at(1) == "world");
  EXPECT_FALSE(SplitStringIntoArray<3>(subject2, '.').has_value());

  constexpr auto subject3 = "hello.universe.";
  EXPECT_FALSE(SplitStringIntoArray<2>(subject3, '.').has_value());
  const auto split3 = SplitStringIntoArray<3>(subject3, '.');
  EXPECT_TRUE(split3.has_value() && split3->at(2) == "");
  EXPECT_FALSE(SplitStringIntoArray<4>(subject3, '.').has_value());

  constexpr auto subject4 = "=";
  EXPECT_TRUE(SplitStringIntoArray<2>(subject4, '=').has_value());
}

TEST(StringUtil, StringToIPv4PortRange)
{
  constexpr auto parse = Common::StringToIPv4PortRange;

  constexpr Common::IPAddress ip_network = {192, 168, 0, 0};
  constexpr Common::IPAddress test_ip = {192, 168, 0, 13};
  constexpr Common::IPAddress last_ip = {192, 168, 0, 255};

  EXPECT_FALSE(parse("").has_value());
  EXPECT_FALSE(parse("1.2.3").has_value());
  EXPECT_FALSE(parse("1.2.3.4-").has_value());
  EXPECT_FALSE(parse("1.2.3.4.5").has_value());
  EXPECT_FALSE(parse("1.2.3.256").has_value());
  EXPECT_FALSE(parse("1.2.3.4/33").has_value());
  EXPECT_FALSE(parse("1.2.3.4:65536").has_value());
  EXPECT_FALSE(parse("1.2.3.4-1.2.3.4.5").has_value());
  EXPECT_FALSE(parse("1.1.1.1:81-80").has_value());
  EXPECT_FALSE(parse("1.1.1.2-1.1.1.1").has_value());

  EXPECT_TRUE(parse("1.2.3.4").has_value());
  EXPECT_TRUE(parse("1.2.3.4:80").has_value());
  EXPECT_TRUE(parse("1.2.3.4:80-81").has_value());
  EXPECT_TRUE(parse("1.2.3.4/16").has_value());
  EXPECT_TRUE(parse("1.2.3.4/16:80").has_value());
  EXPECT_TRUE(parse("1.2.3.4/16:80-81").has_value());
  EXPECT_TRUE(parse("1.2.3.4-10").has_value());
  EXPECT_TRUE(parse("1.2.3.4-10:80").has_value());
  EXPECT_TRUE(parse("1.2.3.4-10:80-81").has_value());

  {
    const auto simple_result = parse("192.168.0.13").value();
    EXPECT_EQ(simple_result.first.ip_address, test_ip);
    EXPECT_EQ(simple_result.last.ip_address, test_ip);
    EXPECT_EQ(simple_result.first.GetPortValue(), 0);
    EXPECT_EQ(simple_result.last.GetPortValue(), u16(-1));
  }

  {
    const auto port_result = parse("192.168.0.13:77").value();
    EXPECT_EQ(port_result.first.ip_address, test_ip);
    EXPECT_EQ(port_result.last.ip_address, test_ip);
    EXPECT_EQ(port_result.first.GetPortValue(), 77);
    EXPECT_EQ(port_result.last.GetPortValue(), 77);
  }

  {
    const auto ip_range_result = parse("192.168.0.13-192.168.0.255:123").value();
    EXPECT_EQ(ip_range_result.first.ip_address, test_ip);
    EXPECT_EQ(ip_range_result.last.ip_address, last_ip);
    EXPECT_EQ(ip_range_result.first.GetPortValue(), 123);
    EXPECT_EQ(ip_range_result.last.GetPortValue(), 123);
  }

  {
    const auto port_range_result = parse("192.168.29.151:5000-5010").value();
    EXPECT_EQ(port_range_result.first.GetPortValue(), 5000);
    EXPECT_EQ(port_range_result.last.GetPortValue(), 5010);
    EXPECT_TRUE(port_range_result.IsMatch(parse("192.168.29.151:5008")->first));
    EXPECT_FALSE(port_range_result.IsMatch(parse("127.0.0.1:5000")->first));
  }

  {
    const auto ip_range_result = parse("192.168.0.13-255:123-999").value();
    EXPECT_EQ(ip_range_result.first.ip_address, test_ip);
    EXPECT_EQ(ip_range_result.last.ip_address, last_ip);
    EXPECT_EQ(ip_range_result.first.GetPortValue(), 123);
    EXPECT_EQ(ip_range_result.last.GetPortValue(), 999);
  }

  {
    const auto ip_network_result = parse("192.168.0.13/24:123-999").value();
    EXPECT_EQ(ip_network_result.first.ip_address, ip_network);
    EXPECT_EQ(ip_network_result.last.ip_address, last_ip);
    EXPECT_EQ(ip_network_result.first.GetPortValue(), 123);
    EXPECT_EQ(ip_network_result.last.GetPortValue(), 999);
  }

  EXPECT_TRUE(parse("192.168.0.13/24")->IsMatch(parse("192.168.0.99")->first));
  EXPECT_FALSE(parse("192.168.0.13/24")->IsMatch(parse("192.168.1.99")->first));

  EXPECT_FALSE(parse("192.168.0.13/32")->IsMatch(parse("192.168.0.99")->first));
  EXPECT_TRUE(parse("192.168.0.13/0")->IsMatch(parse("10.0.0.1")->first));

  EXPECT_TRUE(parse("192.168.0.13")->IsMatch(parse("192.168.0.13:81")->first));
  EXPECT_FALSE(parse("192.168.0.13:80")->IsMatch(parse("192.168.0.13:81")->first));
  EXPECT_TRUE(parse("192.168.0.13:80")->IsMatch(parse("192.168.0.13:80")->first));
  EXPECT_FALSE(parse("192.168.0.13:80-81")->IsMatch(parse("192.168.0.14:81")->first));
  EXPECT_TRUE(parse("192.168.0.13-14:80-81")->IsMatch(parse("192.168.0.14:81")->first));
}

TEST(StringUtil, CharacterEncodingConversion)
{
  // wstring
  EXPECT_EQ(WStringToUTF8(L"hello 🐬"), "hello 🐬");

  // UTF-16
  EXPECT_EQ(UTF16ToUTF8(u"hello 🐬"), "hello 🐬");
  EXPECT_EQ(UTF8ToUTF16("hello 🐬"), u"hello 🐬");

  // UTF-16BE
  char16_t utf16be_str[] = u"hello 🐬";
  for (auto& c : utf16be_str)
    c = Common::swap16(c);
  EXPECT_EQ(UTF16BEToUTF8(utf16be_str, 99), "hello 🐬");

  // Shift JIS
  EXPECT_EQ(SHIFTJISToUTF8("\x83\x43\x83\x8b\x83\x4a"), "イルカ");
  EXPECT_EQ(UTF8ToSHIFTJIS("イルカ"), "\x83\x43\x83\x8b\x83\x4a");

  // CP1252
  EXPECT_EQ(CP1252ToUTF8("hello \xa5"), "hello ¥");
}
