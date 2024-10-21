// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

#include "Common/Config/Config.h"

class ConfigChoice : public ToolTipComboBox
{
  Q_OBJECT
public:
  ConfigChoice(const QStringList& options, const Config::Info<int>& setting);

private:
  void Update(int choice);

  Config::Info<int> m_setting;
};

class ConfigStringChoice : public ToolTipComboBox
{
  Q_OBJECT
public:
  ConfigStringChoice(const std::vector<std::string>& options,
                     const Config::Info<std::string>& setting);
  ConfigStringChoice(const std::vector<std::pair<QString, QString>>& options,
                     const Config::Info<std::string>& setting);

private:
  void Connect();
  void Update(int index);
  void Load();

  Config::Info<std::string> m_setting;
  bool m_text_is_data = false;
};

class ConfigComplexChoice final : public ToolTipComboBox
{
  Q_OBJECT

  using InfoVariant = std::variant<Config::Info<u32>, Config::Info<int>, Config::Info<bool>>;
  using OptionVariant = std::variant<u32, int, bool>;

public:
  ConfigComplexChoice(const InfoVariant setting1, const InfoVariant setting2);

  void Add(const QString& name, const OptionVariant option1, const OptionVariant option2);
  void Refresh();
  void Reset();

private:
  void SaveValue(int choice);
  void UpdateComboIndex();
  const std::pair<Config::Location, Config::Location> GetLocation() const;

  InfoVariant m_setting1;
  InfoVariant m_setting2;
  std::vector<std::pair<OptionVariant, OptionVariant>> m_options;
};
