// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFont>
#include <QMouseEvent>
#include <QSignalBlocker>

#include "Common/Config/Enums.h"
#include "DolphinQt/Settings.h"

namespace Config
{
template <typename T>
class Info;
struct Location;
}  // namespace Config

template <class Derived>
class ConfigControl : public Derived
{
public:
  ConfigControl(const Config::Location& location) : m_location(location) { ConnectConfig(); }
  ConfigControl(const QString& label, const Config::Location& location)
      : Derived(label), m_location(location)
  {
    ConnectConfig();
  }
  ConfigControl(const Qt::Orientation& orient, const Config::Location& location)
      : Derived(orient), m_location(location)
  {
    ConnectConfig();
  }

  const Config::Location GetLocation() const { return m_location; }

protected:
  void ConnectConfig()
  {
    Derived::connect(&Settings::Instance(), &Settings::ConfigChanged, this, [this] {
      QFont bf = Derived::font();
      bf.setBold(IsConfigLocal());
      Derived::setFont(bf);

      const QSignalBlocker blocker(this);
      OnConfigChanged();
    });
  }

  template <typename T>
  void SaveValue(const Config::Info<T>& setting, const T& value)
  {
    Config::SetBaseOrCurrent(setting, value);
  }

  template <typename T>
  const T ReadValue(const Config::Info<T>& setting) const
  {
    return Config::Get(setting);
  }

  virtual void OnConfigChanged() {};

private:
  bool IsConfigLocal() const
  {
    return Config::GetActiveLayerForConfig(m_location) != Config::LayerType::Base;
  }

  const Config::Location m_location;
};
