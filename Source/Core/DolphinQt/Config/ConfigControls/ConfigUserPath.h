// Copyright 2025 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QLineEdit>

#include "Common/Config/ConfigInfo.h"

#include "DolphinQt/Config/ConfigControls/ConfigText.h"

class ConfigUserPath final : public ConfigText
{
  Q_OBJECT
public:
  ConfigUserPath(const unsigned int dir_index, const Config::Info<std::string>& setting);
  ConfigUserPath(const unsigned int dir_index, const Config::Info<std::string>& setting,
                 std::string default_value);
  ConfigUserPath(const unsigned int dir_index, const Config::Info<std::string>& setting,
                 std::string default_value, Config::Layer* layer);
  void Reset();

protected:
  void OnConfigChanged() override;

private:
  void RefreshText();
  void Update();

  const unsigned int m_dir_index;
  const std::string m_default_value;
};
