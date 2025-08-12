// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLineEdit>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"

#include "Common/Config/ConfigInfo.h"

class ConfigText : public ConfigControl<QLineEdit>
{
  Q_OBJECT
public:
  ConfigText(const Config::Info<std::string>& setting);
  ConfigText(const Config::Info<std::string>& setting, Config::Layer* layer);

  void SetTextAndUpdate(const QString& text);

protected:
  void OnConfigChanged() override;

  const Config::Info<std::string> m_setting;

private:
  void Update();
};
