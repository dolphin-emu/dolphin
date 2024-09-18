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
  const QSignalBlocker block(this);
  setCurrentIndex(index);
}

void ConfigStringChoice::OnConfigChanged()
{
  Load();
}
