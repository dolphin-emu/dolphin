// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <map>

#include <QObject>
#include <QTimer>
#include <QWidget>

#include "Core/API/Gui.h"

// Manages free-floating OS windows for non-embedded script GUI windows.
// Polls API::Gui's widget tree on a timer; Qt signals write results back.
class ScriptWindowManager : public QObject
{
  Q_OBJECT
public:
  explicit ScriptWindowManager(QObject* parent = nullptr);
  ~ScriptWindowManager();

private:
  void Sync();

  struct ManagedWindow
  {
    API::Gui::WidgetId id;
    QWidget* window;
    std::map<API::Gui::WidgetId, QWidget*> children;
  };

  std::map<API::Gui::WidgetId, ManagedWindow> m_windows;
  QTimer m_timer;
};
