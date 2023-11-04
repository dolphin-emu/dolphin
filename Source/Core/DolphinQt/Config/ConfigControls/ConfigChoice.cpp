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
    : m_setting(setting)
{
  for (const auto& op : options)
    addItem(QString::fromStdString(op));

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
  Config::SetBaseOrCurrent(m_setting, itemText(index).toStdString());
}

void ConfigStringChoice::Load()
{
  const QSignalBlocker blocker(this);
  setCurrentIndex(findText(QString::fromStdString(Config::Get(m_setting))));
}
