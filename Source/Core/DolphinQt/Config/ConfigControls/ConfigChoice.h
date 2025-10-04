// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

#include "Common/Config/ConfigInfo.h"

class ConfigChoice final : public ConfigControl<ToolTipComboBox>
{
  Q_OBJECT
public:
  ConfigChoice(const QStringList& options, const Config::Info<int>& setting,
               Config::Layer* layer = nullptr);

protected:
  void OnConfigChanged() override;

private:
  void Update(int choice);

  Config::Info<int> m_setting;
};

class ConfigChoiceU32 final : public ConfigControl<ToolTipComboBox>
{
  Q_OBJECT
public:
  ConfigChoiceU32(const QStringList& options, const Config::Info<u32>& setting,
                  Config::Layer* layer = nullptr);

protected:
  void OnConfigChanged() override;

private:
  void Update(int choice);

  Config::Info<u32> m_setting;
};

template <typename T>
class ConfigChoiceMap final : public ConfigControl<ToolTipComboBox>
{
public:
  ConfigChoiceMap(const std::vector<std::pair<QString, T>>& options, const Config::Info<T>& setting,
                  Config::Layer* layer = nullptr)
      : ConfigControl<ToolTipComboBox>(setting.GetLocation(), layer), m_setting(setting),
        m_options(options)
  {
    for (const auto& [option_text, option_data] : options)
      addItem(option_text);

    OnConfigChanged();
    connect(this, &QComboBox::currentIndexChanged, this, &ConfigChoiceMap::Update);
  }

protected:
  void OnConfigChanged() override
  {
    const T value = ReadValue(m_setting);
    const auto it = std::find_if(m_options.begin(), m_options.end(),
                                 [&value](const auto& pair) { return pair.second == value; });

    int index =
        (it != m_options.end()) ? static_cast<int>(std::distance(m_options.begin(), it)) : -1;

    const QSignalBlocker blocker(this);
    setCurrentIndex(index);
  }

private:
  void Update(int choice) { SaveValue(m_setting, m_options[choice].second); }

  const Config::Info<T> m_setting;
  const std::vector<std::pair<QString, T>> m_options;
};

class ConfigStringChoice final : public ConfigControl<ToolTipComboBox>
{
  Q_OBJECT
public:
  ConfigStringChoice(const std::vector<std::string>& options,
                     const Config::Info<std::string>& setting, Config::Layer* layer = nullptr);
  ConfigStringChoice(const std::vector<std::pair<QString, QString>>& options,
                     const Config::Info<std::string>& setting, Config::Layer* layer = nullptr);
  void Load();

protected:
  void OnConfigChanged() override;

private:
  void Update(int index);

  const Config::Info<std::string> m_setting;
  bool m_text_is_data = false;
};

class ConfigComplexChoice final : public ToolTipComboBox
{
  Q_OBJECT

  using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>;
  using OptionVariant = std::variant<Config::DefaultState, u32, int, bool>;

public:
  ConfigComplexChoice(const InfoVariant setting1, const InfoVariant setting2,
                      Config::Layer* layer = nullptr);

  void Add(const QString& name, const OptionVariant option1, const OptionVariant option2);
  void Refresh();
  void Reset();
  void SetDefault(int index);
  const std::pair<Config::Location, Config::Location> GetLocation() const;

private:
  void SaveValue(int choice);
  void UpdateComboIndex();
  void mousePressEvent(QMouseEvent* event) override;

  Config::Layer* m_layer;
  const InfoVariant m_setting1;
  const InfoVariant m_setting2;
  std::vector<std::pair<OptionVariant, OptionVariant>> m_options;
  int m_default_index = -1;
};
