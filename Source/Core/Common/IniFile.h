// Copyright 2008 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstring>
#include <list>
#include <map>
#include <string>
#include <vector>

#include "Common/CommonFuncs.h"
#include "Common/CommonTypes.h"

struct CaseInsensitiveStringCompare
{
  bool operator()(const std::string& a, const std::string& b) const
  {
    return strcasecmp(a.c_str(), b.c_str()) < 0;
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
    bool Exists(const std::string& key) const;
    bool Delete(const std::string& key);

    void Set(const std::string& key, const std::string& newValue);
    void Set(const std::string& key, const std::string& newValue, const std::string& defaultValue);
    void Set(const std::string& key, u32 newValue);
    void Set(const std::string& key, u64 new_value);
    void Set(const std::string& key, float newValue);
    void Set(const std::string& key, double newValue);
    void Set(const std::string& key, int newValue);
    void Set(const std::string& key, s64 new_value);
    void Set(const std::string& key, bool newValue);

    template <typename T>
    void Set(const std::string& key, T newValue, const T defaultValue)
    {
      if (newValue != defaultValue)
        Set(key, newValue);
      else
        Delete(key);
    }

    void Set(const std::string& key, const std::vector<std::string>& newValues);

    bool Get(const std::string& key, std::string* value,
             const std::string& defaultValue = NULL_STRING) const;
    bool Get(const std::string& key, int* value, int defaultValue = 0) const;
    bool Get(const std::string& key, s64* value, s64 default_value = 0) const;
    bool Get(const std::string& key, u32* value, u32 defaultValue = 0) const;
    bool Get(const std::string& key, u64* value, u64 default_value = 0) const;
    bool Get(const std::string& key, bool* value, bool defaultValue = false) const;
    bool Get(const std::string& key, float* value, float defaultValue = 0.0f) const;
    bool Get(const std::string& key, double* value, double defaultValue = 0.0) const;
    bool Get(const std::string& key, std::vector<std::string>* values) const;

    void SetLines(const std::vector<std::string>& lines);
    void SetLines(std::vector<std::string>&& lines);
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

  // Returns true if key exists in section
  bool Exists(const std::string& sectionName, const std::string& key) const;

  template <typename T>
  bool GetIfExists(const std::string& sectionName, const std::string& key, T* value)
  {
    if (Exists(sectionName, key))
      return GetOrCreateSection(sectionName)->Get(key, value);

    return false;
  }

  template <typename T>
  bool GetIfExists(const std::string& sectionName, const std::string& key, T* value, T defaultValue)
  {
    if (Exists(sectionName, key))
      return GetOrCreateSection(sectionName)->Get(key, value, defaultValue);
    else
      *value = defaultValue;

    return false;
  }

  bool GetKeys(const std::string& sectionName, std::vector<std::string>* keys) const;

  void SetLines(const std::string& sectionName, const std::vector<std::string>& lines);
  void SetLines(const std::string& section_name, std::vector<std::string>&& lines);
  bool GetLines(const std::string& sectionName, std::vector<std::string>* lines,
                const bool remove_comments = true) const;

  bool DeleteKey(const std::string& sectionName, const std::string& key);
  bool DeleteSection(const std::string& sectionName);

  void SortSections();

  Section* GetOrCreateSection(const std::string& section);

  // This function is related to parsing data from lines of INI files
  // It's used outside of IniFile, which is why it is exposed publicly
  // In particular it is used in PostProcessing for its configuration
  static void ParseLine(const std::string& line, std::string* keyOut, std::string* valueOut);

  const std::list<Section>& GetSections() const { return sections; }
private:
  std::list<Section> sections;

  const Section* GetSection(const std::string& section) const;
  Section* GetSection(const std::string& section);

  static const std::string& NULL_STRING;
};
