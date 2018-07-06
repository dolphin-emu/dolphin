// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <array>

#include <QEvent>
#include <QWidget>

class QMouseEvent;
class QTimer;

class RenderWidget final : public QWidget
{
  Q_OBJECT

public:
  explicit RenderWidget(QWidget* parent = nullptr);

  bool event(QEvent* event) override;
  void showFullScreen();

signals:
  void EscapePressed();
  void Closed();
  void HandleChanged(void* handle);
  void StateChanged(bool fullscreen);
  void SizeChanged(int new_width, int new_height);
  void FocusChanged(bool focus);

private:
  void HandleCursorTimer();
  void OnHideCursorChanged();
  void OnKeepOnTopChanged(bool top);
  void SetFillBackground(bool fill);
  void OnFreeLookMouseMove(QMouseEvent* event);

  static constexpr int MOUSE_HIDE_DELAY = 3000;
  QTimer* m_mouse_timer;
  std::array<float, 2> m_last_mouse{};
};
