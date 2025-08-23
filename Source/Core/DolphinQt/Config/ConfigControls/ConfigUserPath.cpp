// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigUserPath.h"

#include <QLineEdit>

#include "Common/FileUtil.h"

#include "DolphinQt/Config/ConfigControls/ConfigText.h"

ConfigUserPath::ConfigUserPath(const unsigned int dir_index,
                               const Config::Info<std::string>& setting)
    : ConfigUserPath(dir_index, setting, nullptr)
{
}

ConfigUserPath::ConfigUserPath(const unsigned int dir_index,
                               const Config::Info<std::string>& setting, Config::Layer* layer)
    : ConfigText(setting, layer), m_dir_index(dir_index)
{
  OnConfigChanged();

  connect(this, &QLineEdit::editingFinished, this, &ConfigUserPath::Update);
}

void ConfigUserPath::Update()
{
  const std::string value = text().toStdString();

  File::SetUserPath(m_dir_index, value);
  SaveValue(m_setting, value);
}

void ConfigUserPath::OnConfigChanged()
{
  const std::string config_value = ReadValue(m_setting);
  const std::string user_value = File::GetUserPath(m_dir_index);
  const std::string value = config_value.empty() ? user_value : config_value;
  setText(QString::fromStdString(value));
}
