// Copyright 2008 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <algorithm>
#include <list>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "Common/CommonTypes.h"
#include "Common/StringUtil.h"

namespace Common
{
struct CaseInsensitiveStringCompare
{
  // Allow heterogenous lookup.
  using is_transparent = void;

  bool operator()(std::string_view a, std::string_view b) const
  {
    return std::ranges::lexicographical_compare(
        a, b, [](char lhs, char rhs) { return Common::ToLower(lhs) < Common::ToLower(rhs); });
  }

  static bool IsEqual(std::string_view a, std::string_view b)
  {
    if (a.size() != b.size())
      return false;

    return std::ranges::equal(
        a, b, [](char lhs, char rhs) { return Common::ToLower(lhs) == Common::ToLower(rhs); });
  }
};

class IniFile
{
public:
  class Section
  {
    friend class IniFile;

  public:
    Section();
    explicit Section(std::string name_);
    bool Exists(std::string_view key) const;
    bool Delete(std::string_view key);

    void Set(const std::string& key, std::string new_value);

    template <typename T>
    void Set(const std::string& key, T&& new_value)
    {
      Set(key, ValueToString(std::forward<T>(new_value)));
    }

    template <typename T>
    void Set(const std::string& key, T&& new_value, const std::common_type_t<T>& default_value)
    {
      if (new_value != default_value)
        Set(key, std::forward<T>(new_value));
      else
        Delete(key);
    }

    bool Get(std::string_view key, std::string* value,
             const std::string& default_value = NULL_STRING) const;

    template <typename T>
    bool Get(std::string_view key, T* value, const std::common_type_t<T>& default_value = {}) const
    {
      std::string temp;
      bool retval = Get(key, &temp);
      if (retval && TryParse(temp, value))
        return true;
      *value = default_value;
      return false;
    }

    void SetLines(std::vector<std::string> lines);
    bool GetLines(std::vector<std::string>* lines, const bool remove_comments = true) const;

    bool operator<(const Section& other) const { return name < other.name; }
    using SectionMap = std::map<std::string, std::string, CaseInsensitiveStringCompare>;

    const std::string& GetName() const { return name; }
    const SectionMap& GetValues() const { return values; }
    bool HasLines() const { return !m_lines.empty(); }

  protected:
    std::string name;

    std::vector<std::string> keys_order;
    SectionMap values;

    std::vector<std::string> m_lines;
  };

  IniFile();
  ~IniFile();

  /**
   * Loads sections and keys.
   * @param filename filename of the ini file which should be loaded
   * @param keep_current_data If true, "extends" the currently loaded list of sections and keys with
   * the loaded data (and replaces existing entries). If false, existing data will be erased.
   * @warning Using any other operations than "Get*" and "Exists" is untested and will behave
   * unexpectedly
   * @todo This really is just a hack to support having two levels of gameinis (defaults and
   * user-specified) and should eventually be replaced with a less stupid system.
   */
  bool Load(const std::string& filename, bool keep_current_data = false);

  bool Save(const std::string& filename);

  bool Exists(std::string_view section_name) const;
  // Returns true if key exists in section
  bool Exists(std::string_view section_name, std::string_view key) const;

  template <typename T>
  bool GetIfExists(std::string_view section_name, std::string_view key, T* value)
  {
    if (Exists(section_name, key))
      return GetOrCreateSection(section_name)->Get(key, value);

    return false;
  }

  template <typename T>
  bool GetIfExists(std::string_view section_name, std::string_view key, T* value, T default_value)
  {
    if (Exists(section_name, key))
      return GetOrCreateSection(section_name)->Get(key, value, default_value);

    *value = default_value;
    return false;
  }

  bool GetKeys(std::string_view section_name, std::vector<std::string>* keys) const;

  void SetLines(std::string_view section_name, std::vector<std::string> lines);
  bool GetLines(std::string_view section_name, std::vector<std::string>* lines,
                bool remove_comments = true) const;

  bool DeleteKey(std::string_view section_name, std::string_view key);
  bool DeleteSection(std::string_view section_name);

  void SortSections();

  Section* GetOrCreateSection(std::string_view section_name);
  const Section* GetSection(std::string_view section_name) const;
  Section* GetSection(std::string_view section_name);

  // This function is related to parsing data from lines of INI files
  // It's used outside of IniFile, which is why it is exposed publicly
  // In particular it is used in PostProcessing for its configuration
  static void ParseLine(std::string_view line, std::string* keyOut, std::string* valueOut);

  const std::list<Section>& GetSections() const { return sections; }

private:
  std::list<Section> sections;

  static const std::string& NULL_STRING;
};
}  // namespace Common
