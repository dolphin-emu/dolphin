// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "Common/IniFile.h"

#include <algorithm>
#include <cstddef>
#include <fstream>
#include <map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Common/FileUtil.h"
#include "Common/StringUtil.h"

namespace Common
{
void IniFile::ParseLine(std::string_view line, std::string* keyOut, std::string* valueOut)
{
  if (line.empty() || line.front() == '#')
    return;

  size_t firstEquals = line.find('=');

  if (firstEquals != std::string::npos)
  {
    // Yes, a valid line!
    *keyOut = StripWhitespace(line.substr(0, firstEquals));

    if (valueOut)
    {
      *valueOut = StripQuotes(StripWhitespace(line.substr(firstEquals + 1, std::string::npos)));
    }
  }
}

const std::string& IniFile::NULL_STRING = "";

IniFile::Section::Section() = default;

IniFile::Section::Section(std::string name_) : name{std::move(name_)}
{
}

void IniFile::Section::Set(const std::string& key, std::string new_value)
{
  const auto result = values.insert_or_assign(key, std::move(new_value));
  const bool insertion_occurred = result.second;

  if (insertion_occurred)
    keys_order.push_back(key);
}

bool IniFile::Section::Get(std::string_view key, std::string* value,
                           const std::string& default_value) const
{
  const auto it = values.find(key);

  if (it != values.end())
  {
    *value = it->second;
    return true;
  }

  if (&default_value != &NULL_STRING)
  {
    *value = default_value;
    return true;
  }

  return false;
}

bool IniFile::Section::Exists(std::string_view key) const
{
  return values.contains(key);
}

bool IniFile::Section::Delete(std::string_view key)
{
  const auto it = values.find(key);
  if (it == values.end())
    return false;

  values.erase(it);
  keys_order.erase(std::ranges::find(keys_order, key));
  return true;
}

void IniFile::Section::SetLines(std::vector<std::string> lines)
{
  m_lines = std::move(lines);
}

bool IniFile::Section::GetLines(std::vector<std::string>* lines, const bool remove_comments) const
{
  for (const std::string& line : m_lines)
  {
    std::string_view stripped_line = StripWhitespace(line);

    if (remove_comments)
    {
      size_t commentPos = stripped_line.find('#');
      if (commentPos == 0)
      {
        continue;
      }

      if (commentPos != std::string::npos)
      {
        stripped_line = StripWhitespace(stripped_line.substr(0, commentPos));
      }
    }

    lines->emplace_back(stripped_line);
  }

  return true;
}

// IniFile

IniFile::IniFile() = default;

IniFile::~IniFile() = default;

const IniFile::Section* IniFile::GetSection(std::string_view section_name) const
{
  for (const Section& sect : sections)
  {
    if (CaseInsensitiveStringCompare::IsEqual(sect.name, section_name))
      return &sect;
  }

  return nullptr;
}

IniFile::Section* IniFile::GetSection(std::string_view section_name)
{
  for (Section& sect : sections)
  {
    if (CaseInsensitiveStringCompare::IsEqual(sect.name, section_name))
      return &sect;
  }

  return nullptr;
}

IniFile::Section* IniFile::GetOrCreateSection(std::string_view section_name)
{
  Section* section = GetSection(section_name);
  if (!section)
  {
    sections.emplace_back(std::string(section_name));
    section = &sections.back();
  }
  return section;
}

bool IniFile::DeleteSection(std::string_view section_name)
{
  Section* s = GetSection(section_name);
  if (!s)
    return false;

  for (auto iter = sections.begin(); iter != sections.end(); ++iter)
  {
    if (&(*iter) == s)
    {
      sections.erase(iter);
      return true;
    }
  }

  return false;
}

bool IniFile::Exists(std::string_view section_name) const
{
  return GetSection(section_name) != nullptr;
}

bool IniFile::Exists(std::string_view section_name, std::string_view key) const
{
  const Section* section = GetSection(section_name);
  if (!section)
    return false;

  return section->Exists(key);
}

void IniFile::SetLines(std::string_view section_name, std::vector<std::string> lines)
{
  Section* section = GetOrCreateSection(section_name);
  section->SetLines(std::move(lines));
}

bool IniFile::DeleteKey(std::string_view section_name, std::string_view key)
{
  Section* section = GetSection(section_name);
  if (!section)
    return false;
  return section->Delete(key);
}

// Return a list of all keys in a section
bool IniFile::GetKeys(std::string_view section_name, std::vector<std::string>* keys) const
{
  const Section* section = GetSection(section_name);
  if (!section)
  {
    return false;
  }
  *keys = section->keys_order;
  return true;
}

// Return a list of all lines in a section
bool IniFile::GetLines(std::string_view section_name, std::vector<std::string>* lines,
                       const bool remove_comments) const
{
  lines->clear();

  const Section* section = GetSection(section_name);
  if (!section)
    return false;

  return section->GetLines(lines, remove_comments);
}

void IniFile::SortSections()
{
  sections.sort();
}

bool IniFile::Load(const std::string& filename, bool keep_current_data)
{
  if (!keep_current_data)
    sections.clear();
  // first section consists of the comments before the first real section

  // Open file
  std::ifstream in;
  File::OpenFStream(in, filename, std::ios::in);

  if (in.fail())
    return false;

  Section* current_section = nullptr;
  bool first_line = true;
  while (!in.eof())
  {
    std::string line_str;
    if (!std::getline(in, line_str))
    {
      if (in.eof())
        return true;
      else
        return false;
    }

    std::string_view line = line_str;

    // Skips the UTF-8 BOM at the start of files. Notepad likes to add this.
    if (first_line && line.substr(0, 3) == "\xEF\xBB\xBF")
      line.remove_prefix(3);
    first_line = false;

#ifndef _WIN32
    // Check for CRLF eol and convert it to LF
    if (!line.empty() && line.back() == '\r')
      line.remove_suffix(1);
#endif

    if (!line.empty())
    {
      if (line[0] == '[')
      {
        size_t endpos = line.find(']');

        if (endpos != std::string::npos)
        {
          // New section!
          std::string_view sub = line.substr(1, endpos - 1);
          current_section = GetOrCreateSection(sub);
        }
      }
      else
      {
        if (current_section)
        {
          std::string key, value;
          ParseLine(line, &key, &value);

          // Lines starting with '$', '*' or '+' are kept verbatim.
          // Kind of a hack, but the support for raw lines inside an
          // INI is a hack anyway.
          if ((key.empty() && value.empty()) ||
              (!line.empty() && (line[0] == '$' || line[0] == '+' || line[0] == '*')))
          {
            current_section->m_lines.emplace_back(line);
          }
          else
          {
            current_section->Set(key, value);
          }
        }
      }
    }
  }

  in.close();
  return true;
}

bool IniFile::Save(const std::string& filename)
{
  std::ofstream out;
  std::string temp = File::GetTempFilenameForAtomicWrite(filename);
  File::OpenFStream(out, temp, std::ios::out);

  if (out.fail())
  {
    return false;
  }

  for (const Section& section : sections)
  {
    if (!section.keys_order.empty() || !section.m_lines.empty())
      out << '[' << section.name << ']' << std::endl;

    if (section.keys_order.empty())
    {
      for (const std::string& s : section.m_lines)
        out << s << std::endl;
    }
    else
    {
      for (const std::string& kvit : section.keys_order)
      {
        auto pair = section.values.find(kvit);
        out << pair->first << " = " << pair->second << std::endl;
      }
    }
  }

  out.close();

  return File::RenameSync(temp, filename);
}

// Unit test. TODO: Move to the real unit test framework.
/*
   int main()
   {
    IniFile ini;
    ini.Load("my.ini");
    ini.Set("Hello", "A", "amaskdfl");
    ini.Set("Moss", "A", "amaskdfl");
    ini.Set("Aissa", "A", "amaskdfl");
    //ini.Read("my.ini");
    std::string x;
    ini.Get("Hello", "B", &x, "boo");
    ini.DeleteKey("Moss", "A");
    ini.DeleteSection("Moss");
    ini.SortSections();
    ini.Save("my.ini");
    //UpdateVars(ini);
    return 0;
   }
 */
}  // namespace Common
