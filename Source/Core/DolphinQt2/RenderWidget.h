// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QEvent>
#include <QWidget>

class QTimer;

class RenderWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit RenderWidget(QWidget* parent = nullptr);

  bool event(QEvent* event);

signals:
  void EscapePressed();
  void Closed();
  void HandleChanged(void* handle);
  void FocusChanged(bool focus);
  void StateChanged(bool fullscreen);

private:
  void HandleCursorTimer();
  void OnHideCursorChanged();

  static constexpr int MOUSE_HIDE_DELAY = 3000;
  QTimer* m_mouse_timer;
};
