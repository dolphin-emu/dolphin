// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigChoice.h"

#include <QSignalBlocker>

ConfigChoice::ConfigChoice(const QStringList& options, const Config::Info<int>& setting,
                           Config::Layer* layer)
    : ConfigControl(setting.GetLocation(), layer), m_setting(setting)
{
  addItems(options);
  setCurrentIndex(ReadValue(setting));

  connect(this, &QComboBox::currentIndexChanged, this, &ConfigChoice::Update);
}

void ConfigChoice::Update(int choice)
{
  SaveValue(m_setting, choice);
}

void ConfigChoice::OnConfigChanged()
{
  setCurrentIndex(ReadValue(m_setting));
}

ConfigStringChoice::ConfigStringChoice(const std::vector<std::string>& options,
                                       const Config::Info<std::string>& setting,
                                       Config::Layer* layer)
    : ConfigControl(setting.GetLocation(), layer), m_setting(setting), m_text_is_data(true)
{
  for (const auto& op : options)
    addItem(QString::fromStdString(op));

  Load();
  connect(this, &QComboBox::currentIndexChanged, this, &ConfigStringChoice::Update);
}

ConfigStringChoice::ConfigStringChoice(const std::vector<std::pair<QString, QString>>& options,
                                       const Config::Info<std::string>& setting,
                                       Config::Layer* layer)
    : ConfigControl(setting.GetLocation(), layer), m_setting(setting), m_text_is_data(false)
{
  for (const auto& [option_text, option_data] : options)
    addItem(option_text, option_data);

  connect(this, &QComboBox::currentIndexChanged, this, &ConfigStringChoice::Update);
  Load();
}

void ConfigStringChoice::Update(int index)
{
  if (m_text_is_data)
    SaveValue(m_setting, itemText(index).toStdString());
  else
    SaveValue(m_setting, itemData(index).toString().toStdString());
}

void ConfigStringChoice::Load()
{
  const QString setting_value = QString::fromStdString(ReadValue(m_setting));
  const int index = m_text_is_data ? findText(setting_value) : findData(setting_value);

  // This can be called publicly.
  setCurrentIndex(index);
}

void ConfigStringChoice::OnConfigChanged()
{
  Load();
}

ConfigComplexChoice::ConfigComplexChoice(const InfoVariant setting1, const InfoVariant setting2,
                                         Config::Layer* layer)
    : m_setting1(setting1), m_setting2(setting2), m_layer(layer)
{
  connect(&Settings::Instance(), &Settings::ConfigChanged, this, &ConfigComplexChoice::Refresh);
  connect(this, &QComboBox::currentIndexChanged, this, &ConfigComplexChoice::SaveValue);
}

void ConfigComplexChoice::Refresh()
{
  auto& location = GetLocation();

  QFont bf = font();
  if (m_layer != nullptr)
  {
    bf.setBold(m_layer->Exists(location.first) || m_layer->Exists(location.second));
  }
  else
  {
    bf.setBold(Config::GetActiveLayerForConfig(location.first) != Config::LayerType::Base ||
               Config::GetActiveLayerForConfig(location.second) != Config::LayerType::Base);
  }

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
    if (m_layer != nullptr)
    {
      m_layer->Set(setting.GetLocation(), value);
      Config::OnConfigChanged();
      return;
    }

    Config::SetBaseOrCurrent(setting, value);
  };

  std::visit(Set, m_setting1, m_options[choice].first);
  std::visit(Set, m_setting2, m_options[choice].second);
}

void ConfigComplexChoice::UpdateComboIndex()
{
  auto Get = [this](auto& setting) {
    if (m_layer != nullptr)
      return static_cast<OptionVariant>(m_layer->Get(setting));

    return static_cast<OptionVariant>(Config::Get(setting));
  };

  std::pair<OptionVariant, OptionVariant> values =
      std::make_pair(std::visit(Get, m_setting1), std::visit(Get, m_setting2));

  auto it = std::find(m_options.begin(), m_options.end(), values);
  int index = static_cast<int>(std::distance(m_options.begin(), it));

  // Will crash if not blocked
  const QSignalBlocker blocker(this);
  setCurrentIndex(index);
}

const std::pair<Config::Location, Config::Location> ConfigComplexChoice::GetLocation() const
{
  auto visit = [](auto& v) { return v.GetLocation(); };

  return {std::visit(visit, m_setting1), std::visit(visit, m_setting2)};
}

void ConfigComplexChoice::mousePressEvent(QMouseEvent* event)
{
  if (event->button() == Qt::RightButton && m_layer != nullptr)
  {
    auto& location = GetLocation();
    m_layer->DeleteKey(location.first);
    m_layer->DeleteKey(location.second);
    Config::OnConfigChanged();
  }
  else
  {
    QComboBox::mousePressEvent(event);
  }
};
