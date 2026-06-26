// Copyright 2018 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QObject>

#include "DolphinQt/Scripting/InputDisplaySkin.h"

class InputDisplayDumper;
class InputDisplayWidget;

// Owns the input-display window and sidecar dumper; lifecycle driven by the Movie menu.
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
  // Forwarded from the widget's close button so the menu toggle can uncheck.
  void closed();

private:
  GCSkin m_skin;
  InputDisplayWidget* m_widget = nullptr;
  std::unique_ptr<InputDisplayDumper> m_dumper;
  bool m_dumping = false;
};
