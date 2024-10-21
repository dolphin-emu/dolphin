// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"

#include <QSignalBlocker>

#include "Common/Config/Config.h"

#include "DolphinQt/Settings.h"

ConfigChoice::ConfigChoice(const QStringList& options, const Config::Info<int>& setting)
    : m_setting(setting)
{
  addItems(options);
  connect(this, &QComboBox::currentIndexChanged, this, &ConfigChoice::Update);
  setCurrentIndex(Config::Get(m_setting));

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    const QSignalBlocker blocker(this);
    setCurrentIndex(Config::Get(m_setting));
  });
}

void ConfigChoice::Update(int choice)
{
  Config::SetBaseOrCurrent(m_setting, choice);
}

ConfigStringChoice::ConfigStringChoice(const std::vector<std::string>& options,
                                       const Config::Info<std::string>& setting)
    : m_setting(setting), m_text_is_data(true)
{
  for (const auto& op : options)
    addItem(QString::fromStdString(op));

  Connect();
  Load();
}

ConfigStringChoice::ConfigStringChoice(const std::vector<std::pair<QString, QString>>& options,
                                       const Config::Info<std::string>& setting)
    : m_setting(setting), m_text_is_data(false)
{
  for (const auto& [option_text, option_data] : options)
    addItem(option_text, option_data);

  Connect();
  Load();
}

void ConfigStringChoice::Connect()
{
  const auto on_config_changed = [this]() {
    QFont bf = font();
    bf.setBold(Config::GetActiveLayerForConfig(m_setting) != Config::LayerType::Base);
    setFont(bf);

    Load();
  };

  connect(&Settings::Instance(), &Settings::ConfigChanged, this, on_config_changed);
  connect(this, &QComboBox::currentIndexChanged, this, &ConfigStringChoice::Update);
}

void ConfigStringChoice::Update(int index)
{
  if (m_text_is_data)
  {
    Config::SetBaseOrCurrent(m_setting, itemText(index).toStdString());
  }
  else
  {
    Config::SetBaseOrCurrent(m_setting, itemData(index).toString().toStdString());
  }
}

void ConfigStringChoice::Load()
{
  const QString setting_value = QString::fromStdString(Config::Get(m_setting));

  const int index = m_text_is_data ? findText(setting_value) : findData(setting_value);
  const QSignalBlocker blocker(this);
  setCurrentIndex(index);
}

ConfigComplexChoice::ConfigComplexChoice(const InfoVariant setting1, const InfoVariant setting2)
    : m_setting1(setting1), m_setting2(setting2)
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &ConfigComplexChoice::Refresh);
  connect(this, &QComboBox::currentIndexChanged, this, &ConfigComplexChoice::SaveValue);
}

void ConfigComplexChoice::Refresh()
{
  auto& location = GetLocation();

  QFont bf = font();
  bf.setBold(Config::GetActiveLayerForConfig(location.first) != Config::LayerType::Base ||
             Config::GetActiveLayerForConfig(location.second) != Config::LayerType::Base);

  setFont(bf);

  UpdateComboIndex();
}

void ConfigComplexChoice::Add(const QString& name, const OptionVariant option1,
                              const OptionVariant option2)
{
  const QSignalBlocker blocker(this);
  addItem(name);
  m_options.push_back(std::make_pair(option1, option2));
}

void ConfigComplexChoice::Reset()
{
  clear();
  m_options.clear();
}

void ConfigComplexChoice::SaveValue(int choice)
{
  auto Set = [this, choice](auto& setting, auto& value) {
    Config::SetBaseOrCurrent(setting, value);
  };

  std::visit(Set, m_setting1, m_options[choice].first);
  std::visit(Set, m_setting2, m_options[choice].second);
}

void ConfigComplexChoice::UpdateComboIndex()
{
  auto Get = [this](auto& setting) { return static_cast<OptionVariant>(Config::Get(setting)); };

  std::pair<OptionVariant, OptionVariant> values =
      std::make_pair(std::visit(Get, m_setting1), std::visit(Get, m_setting2));
  auto it = std::find(m_options.begin(), m_options.end(), values);
  int index = static_cast<int>(std::distance(m_options.begin(), it));

  const QSignalBlocker blocker(this);
  setCurrentIndex(index);
}

const std::pair<Config::Location, Config::Location> ConfigComplexChoice::GetLocation() const
{
  auto visit = [](auto& v) { return v.GetLocation(); };
  return {std::visit(visit, m_setting1), std::visit(visit, m_setting2)};
}
