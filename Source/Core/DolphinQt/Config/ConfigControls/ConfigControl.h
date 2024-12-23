// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFont>
#include <QMouseEvent>
#include <QSignalBlocker>

#include "Common/Config/Enums.h"
#include "Common/Config/Layer.h"
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
  ConfigControl(const Config::Location& location, Config::Layer* layer)
      : m_location(location), m_layer(layer)
  {
    ConnectConfig();
  }
  ConfigControl(const QString& label, const Config::Location& location, Config::Layer* layer)
      : Derived(label), m_location(location), m_layer(layer)
  {
    ConnectConfig();
  }
  ConfigControl(const Qt::Orientation& orient, const Config::Location& location,
                Config::Layer* layer)
      : Derived(orient), m_location(location), m_layer(layer)
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
    if (m_layer != nullptr)
    {
      m_layer->Set(m_location, value);
      Config::OnConfigChanged();
      return;
    }

    Config::SetBaseOrCurrent(setting, value);
  }

  template <typename T>
  const T ReadValue(const Config::Info<T>& setting) const
  {
    if (m_layer != nullptr)
      return m_layer->Get(setting);

    return Config::Get(setting);
  }

  virtual void OnConfigChanged(){};

private:
  bool IsConfigLocal() const
  {
    if (m_layer != nullptr)
      return m_layer->Exists(m_location);
    else
      return Config::GetActiveLayerForConfig(m_location) != Config::LayerType::Base;
  }

  void mousePressEvent(QMouseEvent* event) override
  {
    if (m_layer != nullptr && event->button() == Qt::RightButton)
    {
      m_layer->DeleteKey(m_location);
      Config::OnConfigChanged();
    }
    else
    {
      Derived::mousePressEvent(event);
    }
  }

  const Config::Location m_location;
  Config::Layer* m_layer;
};
