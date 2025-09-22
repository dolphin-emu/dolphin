// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ConfigControls/ConfigText.h"

#include <QLineEdit>

ConfigText::ConfigText(const Config::Info<std::string>& setting) : ConfigText(setting, nullptr)
{
}

ConfigText::ConfigText(const Config::Info<std::string>& setting, Config::Layer* layer)
    : ConfigControl(setting.GetLocation(), layer), m_setting(setting)
{
  setText(QString::fromStdString(ReadValue(setting)));

  connect(this, &QLineEdit::editingFinished, this, &ConfigText::Update);
}

void ConfigText::SetTextAndUpdate(const QString& text)
{
  if (text == this->text())
    return;

  setText(text);
  Update();
}

void ConfigText::Update()
{
  const std::string value = text().toStdString();

  SaveValue(m_setting, value);
}

void ConfigText::OnConfigChanged()
{
  setText(QString::fromStdString(ReadValue(m_setting)));
}
