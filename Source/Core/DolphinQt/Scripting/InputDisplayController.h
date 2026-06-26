// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>

#include <QHash>
#include <QObject>
#include <QString>

#include "Common/Config/Config.h"
#include "DolphinQt/Scripting/InputDisplayDumper.h"
#include "DolphinQt/Scripting/InputDisplaySkin.h"

class InputDisplayWidget;

// Owns one input-display window per connected controller port; lifecycle driven by the Movie menu.
// Tracks controller-config changes so ports added/removed in settings open/close their windows live.
class InputDisplayController : public QObject
{
  Q_OBJECT
public:
  explicit InputDisplayController(QObject* parent = nullptr);
  ~InputDisplayController();

  void Show();
  void Hide();
  bool IsShown() const;

  void StartDump();
  void StopDump();
  bool IsDumping() const;

signals:
  // Emitted when all port windows are closed so the menu toggle can uncheck.
  void closed();

private:
  void SyncWindows();
  void OpenPort(int port, const QString& skin_name);
  void ClosePort(int port);
  void OnWidgetClosed(int port);

  QHash<QString, GCSkin> m_skins;
  std::array<InputDisplayWidget*, 4> m_widgets{};
  std::array<QString, 4> m_widget_skins{};  // skin name each open window was built with
  std::array<std::unique_ptr<InputDisplayDumper>, 4> m_dumpers{};
  Config::ConfigChangedCallbackID m_config_callback{};
  bool m_active = false;
  bool m_dumping = false;
};
