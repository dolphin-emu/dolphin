// Copyright 2024 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QFont>
#include <QMouseEvent>

#include "Common/Config/Enums.h"
#include "Common/Config/Layer.h"
#include "DolphinQt/Settings.h"

namespace Config
{
template <typename T>
class Info;
struct Location;
}  // namespace Config

class QShowEvent;

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
      UpdateBoldness();

      // This isn't signal blocked because the UI may need to be updated.
      m_updating = true;
      OnConfigChanged();
      m_updating = false;
    });
  }

  template <typename T>
  void SaveValue(const Config::Info<T>& setting, const T& value)
  {
    // Avoid OnConfigChanged -> option changed to current config's value -> unnecessary save ->
    // ConfigChanged.
    if (m_updating)
      return;

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
    // For loading game specific settings.  If the game setting doesn't exist, load the current
    // global setting. There's no way to know what game is being edited, so GlobalGame settings
    // can't be shown, but otherwise would be good to include.
    if (m_layer != nullptr)
    {
      if (m_layer->Exists(m_location))
        return m_layer->Get(setting);
      else
        return Config::GetBase(setting);
    }

    return Config::Get(setting);
  }

  virtual void OnConfigChanged() {}

  void UpdateBoldness()
  {
    QFont bf = Derived::font();
    bf.setBold(ShouldLabelBeBold());
    Derived::setFont(bf);
  }

  virtual bool ShouldLabelBeBold() const
  {
    if (m_layer != nullptr)
      return m_layer->Exists(m_location);
    else
      return Config::GetActiveLayerForConfig(m_location) != Config::LayerType::Base;
  }

  void showEvent(QShowEvent* const event) override
  {
    // Call this here instead of in the constructor because virtual function calls are disabled in
    // constructors, since base classes are constructed before derived classes and so the derived
    // function would potentially use uninitialized data (as would be the case for
    // ConfigRadioInt::UpdateBoldness).
    UpdateBoldness();
    Derived::showEvent(event);
  }

private:
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

  bool m_updating = false;
  const Config::Location m_location;
  Config::Layer* m_layer;
};
