// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigUserPath.h"

#include <QLineEdit>
#include <QString>

#include "Common/FileUtil.h"

#include "DolphinQt/Config/ConfigControls/ConfigText.h"
#include "DolphinQt/QtUtils/ModalMessageBox.h"

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

// Display the effective path: if Config has a value, use it; otherwise fall back to UserPath.
// Config values only serve to initialize UserPath at startup, an empty config meaning "use the
// current UserPath".
void ConfigUserPath::RefreshText()
{
  const std::string config_value = ReadValue(m_setting);
  const std::string userpath_value = File::GetUserPath(m_dir_index);
  const QString text = QString::fromStdString(config_value.empty() ? userpath_value : config_value);
  setText(text);
}

void ConfigUserPath::Update()
{
  const QString new_text = text().trimmed();
  if (new_text.isEmpty())
  {
    ModalMessageBox::warning(this, tr("Empty Value"),
                             tr("This field cannot be left empty. Please enter a value."));
    RefreshText();
    return;
  }

  const std::string value = new_text.toStdString();
  File::SetUserPath(m_dir_index, value);
  SaveValue(m_setting, value);
}

void ConfigUserPath::OnConfigChanged()
{
  RefreshText();
}
