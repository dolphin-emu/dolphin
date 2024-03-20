// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Translation.h"

#include <algorithm>
#include <cstring>
#include <iterator>
#include <string>

#include <fmt/format.h>

#include <QApplication>
#include <QLocale>
#include <QTranslator>

#include "Common/FileUtil.h"
#include "Common/IOFile.h"
#include "Common/Logging/Log.h"
#include "Common/MsgHandler.h"
#include "Common/StringUtil.h"

#include "Core/Config/MainSettings.h"

#include "DolphinQt/QtUtils/ModalMessageBox.h"

#include "UICommon/UICommon.h"

constexpr u32 MO_MAGIC_NUMBER = 0x950412de;

static u16 ReadU16(const char* data)
{
  u16 value;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

static u32 ReadU32(const char* data)
{
  u32 value;
  std::memcpy(&value, data, sizeof(value));
  return value;
}

class MoIterator
{
public:
  using iterator_category = std::random_access_iterator_tag;
  using value_type = const char*;
  using difference_type = s64;
  using pointer = value_type;
  using reference = value_type;

  explicit MoIterator(const char* data, u32 table_offset, u32 index = 0)
      : m_data{data}, m_table_offset{table_offset}, m_index{index}
  {
  }

  // This is the actual underlying logic of accessing a Mo file. Patterned after the
  // boost::iterator_facade library, which nicely separates out application logic from
  // iterator-concept logic.
  void advance(difference_type n) { m_index += n; }
  difference_type distance_to(const MoIterator& other) const
  {
    return static_cast<difference_type>(other.m_index) - m_index;
  }
  reference dereference() const
  {
    u32 offset = ReadU32(&m_data[m_table_offset + m_index * 8 + 4]);
    return &m_data[offset];
  }

  // Needed for Iterator concept
  reference operator*() const { return dereference(); }
  MoIterator& operator++()
  {
    advance(1);
    return *this;
  }

  // Needed for InputIterator concept
  bool operator==(const MoIterator& other) const { return distance_to(other) == 0; }
  bool operator!=(const MoIterator& other) const { return !(*this == other); }
  pointer operator->() const { return dereference(); }
  MoIterator operator++(int)
  {
    MoIterator tmp(*this);
    advance(1);
    return tmp;
  }

  // Needed for BidirectionalIterator concept
  MoIterator& operator--()
  {
    advance(-1);
    return *this;
  }
  MoIterator operator--(int)
  {
    MoIterator tmp(*this);
    advance(-1);
    return tmp;
  }

  // Needed for RandomAccessIterator concept
  bool operator<(const MoIterator& other) const { return distance_to(other) > 0; }
  bool operator<=(const MoIterator& other) const { return distance_to(other) >= 0; }
  bool operator>(const MoIterator& other) const { return distance_to(other) < 0; }
  bool operator>=(const MoIterator& other) const { return distance_to(other) <= 0; }
  reference operator[](difference_type n) const { return *(*this + n); }
  MoIterator& operator+=(difference_type n)
  {
    advance(n);
    return *this;
  }
  MoIterator& operator-=(difference_type n)
  {
    advance(-n);
    return *this;
  }
  friend MoIterator operator+(difference_type n, const MoIterator& it) { return it + n; }
  friend MoIterator operator+(const MoIterator& it, difference_type n)
  {
    MoIterator tmp(it);
    tmp += n;
    return tmp;
  }
  difference_type operator-(const MoIterator& other) const { return other.distance_to(*this); }
  friend MoIterator operator-(difference_type n, const MoIterator& it) { return it - n; }
  friend MoIterator operator-(const MoIterator& it, difference_type n)
  {
    MoIterator tmp(it);
    tmp -= n;
    return tmp;
  }

private:
  const char* m_data;
  u32 m_table_offset;
  u32 m_index;
};

class MoFile
{
public:
  MoFile() = default;
  explicit MoFile(const std::string& filename)
  {
    File::IOFile file(filename, "rb");
    m_data.resize(file.GetSize());
    file.ReadBytes(m_data.data(), m_data.size());

    if (!file)
    {
      WARN_LOG_FMT(COMMON, "Error reading MO file '{}'", filename);
      m_data = {};
      return;
    }

    const u32 magic = ReadU32(&m_data[0]);
    if (magic != MO_MAGIC_NUMBER)
    {
      ERROR_LOG_FMT(COMMON, "MO file '{}' has bad magic number {:x}\n", filename, magic);
      m_data = {};
      return;
    }

    const u16 version_major = ReadU16(&m_data[4]);
    if (version_major > 1)
    {
      ERROR_LOG_FMT(COMMON, "MO file '{}' has unsupported version number {}", filename,
                    version_major);
      m_data = {};
      return;
    }

    m_number_of_strings = ReadU32(&m_data[8]);
    m_offset_original_table = ReadU32(&m_data[12]);
    m_offset_translation_table = ReadU32(&m_data[16]);
  }

  u32 GetNumberOfStrings() const { return m_number_of_strings; }
  const char* Translate(const char* original_string) const
  {
    const MoIterator begin(m_data.data(), m_offset_original_table);
    const MoIterator end(m_data.data(), m_offset_original_table, m_number_of_strings);
    auto iter = std::lower_bound(begin, end, original_string,
                                 [](const char* a, const char* b) { return strcmp(a, b) < 0; });

    if (iter == end || strcmp(*iter, original_string) != 0)
      return nullptr;

    u32 offset = ReadU32(&m_data[m_offset_translation_table + std::distance(begin, iter) * 8 + 4]);
    return &m_data[offset];
  }

private:
  std::vector<char> m_data;
  u32 m_number_of_strings = 0;
  u32 m_offset_original_table = 0;
  u32 m_offset_translation_table = 0;
};

class MoTranslator : public QTranslator
{
public:
  using QTranslator::QTranslator;

  bool isEmpty() const override { return m_mo_file.GetNumberOfStrings() == 0; }
  bool load(const std::string& filename)
  {
    m_mo_file = MoFile(filename);
    return !isEmpty();
  }

  QString translate(const char* context, const char* source_text,
                    const char* disambiguation = nullptr, int n = -1) const override
  {
    const char* translated_text;

    if (disambiguation)
    {
      std::string combined_string = disambiguation;
      combined_string += '\4';
      combined_string += source_text;
      translated_text = m_mo_file.Translate(combined_string.c_str());
    }
    else
    {
      translated_text = m_mo_file.Translate(source_text);
    }

    return QString::fromUtf8(translated_text ? translated_text : source_text);
  }

private:
  MoFile m_mo_file;
};

static QStringList FindPossibleLanguageCodes(const QString& exact_language_code)
{
  QStringList possible_language_codes;
  possible_language_codes << exact_language_code;

  // Qt likes to separate language, script, and country by hyphen, but on disk they're separated by
  // underscores.
  possible_language_codes.replaceInStrings(QStringLiteral("-"), QStringLiteral("_"));

  // Try successively dropping subtags (like the stock QTranslator, and as specified by RFC 4647
  // "Matching of Language Tags").
  // Example: fr_Latn_CA -> fr_Latn -> fr
  for (auto lang : QStringList(possible_language_codes))
  {
    while (lang.contains(QLatin1Char('_')))
    {
      lang = lang.left(lang.lastIndexOf(QLatin1Char('_')));
      possible_language_codes << lang;
    }
  }

  // On macOS, Chinese (Simplified) and Chinese (Traditional) are represented as zh-Hans and
  // zh-Hant, but on Linux they're represented as zh-CN and zh-TW. Qt should probably include the
  // script subtags on Linux, but it doesn't.
  const int hans_index = possible_language_codes.indexOf(QStringLiteral("zh_Hans"));
  if (hans_index != -1)
    possible_language_codes.insert(hans_index + 1, QStringLiteral("zh_CN"));
  const int hant_index = possible_language_codes.indexOf(QStringLiteral("zh_Hant"));
  if (hant_index != -1)
    possible_language_codes.insert(hant_index + 1, QStringLiteral("zh_TW"));

  return possible_language_codes;
}

static bool TryInstallTranslator(const QString& exact_language_code)
{
  for (const auto& qlang : FindPossibleLanguageCodes(exact_language_code))
  {
    std::string lang = qlang.toStdString();
    auto filename =
#if defined _WIN32
        fmt::format("{}/Languages/{}.mo", File::GetExeDirectory(), lang)
#elif defined __APPLE__
        fmt::format("{}/Contents/Resources/{}.lproj/dolphin-emu.mo", File::GetBundleDirectory(),
                    lang)
#elif defined LINUX_LOCAL_DEV
        fmt::format("{}/../Source/Core/DolphinQt/{}/dolphin-emu.mo", File::GetExeDirectory(), lang)
#else
        fmt::format("{}/../locale/{}/LC_MESSAGES/dolphin-emu.mo", DATA_DIR, lang)
#endif
        ;

    auto* translator = new MoTranslator(QApplication::instance());
    if (translator->load(filename))
    {
      QApplication::instance()->installTranslator(translator);

      QLocale::setDefault(QLocale(exact_language_code));
      UICommon::SetLocale(exact_language_code.toStdString());
      QApplication::setLayoutDirection(QLocale(exact_language_code).textDirection());

      return true;
    }
    translator->deleteLater();
  }
  ERROR_LOG_FMT(COMMON, "No suitable translation file found");
  return false;
}

void Translation::Initialize()
{
  // Hook up Dolphin internal translation
  Common::RegisterStringTranslator(
      [](const char* text) { return QObject::tr(text).toStdString(); });

  // Hook up Qt translations
  std::string configured_language = Config::Get(Config::MAIN_INTERFACE_LANGUAGE);
  if (!configured_language.empty())
  {
    if (TryInstallTranslator(QString::fromStdString(configured_language)))
      return;

    ModalMessageBox::warning(
        nullptr, QObject::tr("Error"),
        QObject::tr("Error loading selected language. Falling back to system default."));
    Config::SetBase(Config::MAIN_INTERFACE_LANGUAGE, "");
  }

  for (const auto& lang : QLocale::system().uiLanguages())
  {
    if (TryInstallTranslator(lang))
      break;
  }
}
