// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <string>
#include <utility>
#include <vector>

#include <QMouseEvent>
#include <QSignalBlocker>

#include "DolphinQt/Config/ConfigControls/ConfigControl.h"
#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

namespace Config
{
template <typename T>
class Info;
}  // namespace Config

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
  Config::Layer* m_layer = nullptr;
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

  Config::Info<std::string> m_setting;
  bool m_text_is_data = false;
};

class BaseConfigComplexChoice : public ToolTipComboBox
{
  // Q_OBJECT cannot be used in a templated class, so split into two classes.
  Q_OBJECT

public:
  BaseConfigComplexChoice(Config::Layer* layer);

  void Refresh();

private:
  void mousePressEvent(QMouseEvent* event) override;

  virtual void SaveValue(int choice){};
  virtual void UpdateComboIndex(){};
  virtual const std::pair<Config::Location, Config::Location> GetLocation() const = 0;

  Config::Layer* m_layer;
};

template <typename T, typename U>
class ConfigComplexChoice final : public BaseConfigComplexChoice
{
public:
  ConfigComplexChoice(const Config::Info<T>& setting1, const Config::Info<U>& setting2,
                      Config::Layer* layer = nullptr)
      : BaseConfigComplexChoice(layer), m_setting1(setting1), m_setting2(setting2), m_layer(layer)
  {
  }

  void Add(const QString& name, const T& option1, const U& option2)
  {
    const QSignalBlocker blocker(this);
    addItem(name);
    m_options.push_back(std::make_pair(option1, option2));
  }

  void Reset()
  {
    clear();
    m_options.clear();
  }

private:
  void SaveValue(int choice) override
  {
    if (m_layer != nullptr)
    {
      m_layer->Set(m_setting1, m_options[choice].first);
      m_layer->Set(m_setting2, m_options[choice].second);
      Config::OnConfigChanged();
      return;
    }

    Config::SetBaseOrCurrent(m_setting1, m_options[choice].first);
    Config::SetBaseOrCurrent(m_setting2, m_options[choice].second);
  }

  void UpdateComboIndex() override
  {
    std::pair<T, U> options;
    if (m_layer == nullptr)
      options = {Config::Get(m_setting1), Config::Get(m_setting2)};
    else
      options = {m_layer->Get(m_setting1), m_layer->Get(m_setting2)};

    auto it = std::find(m_options.begin(), m_options.end(), options);

    if (it != m_options.end())
      setCurrentIndex(static_cast<int>(std::distance(m_options.begin(), it)));
  }

  const std::pair<Config::Location, Config::Location> GetLocation() const override
  {
    return {m_setting1.GetLocation(), m_setting2.GetLocation()};
  }

  Config::Info<T> m_setting1;
  Config::Info<U> m_setting2;
  std::vector<std::pair<T, U>> m_options;
  Config::Layer* m_layer;
};
