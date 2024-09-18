// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

namespace Config
{
template <typename T>
class Info;
}

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

  const Config::Info<std::string>& m_setting;
  bool m_text_is_data = false;
};
