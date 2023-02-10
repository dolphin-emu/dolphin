// Copyright 2015 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

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
  QPaintEngine* paintEngine() const override;
  bool IsCursorLocked() const { return m_cursor_locked; }
  void SetCursorLockedOnNextActivation(bool locked = true);
  void SetWaitingForMessageBox(bool waiting_for_message_box);
  void SetCursorLocked(bool locked, bool follow_aspect_ratio = true);

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
  void OnNeverHideCursorChanged();
  void OnLockCursorChanged();
  void OnKeepOnTopChanged(bool top);
  void UpdateCursor();
  void PassEventToPresenter(const QEvent* event);
  void SetPresenterKeyMap();
  void dragEnterEvent(QDragEnterEvent* event) override;
  void dropEvent(QDropEvent* event) override;

  static constexpr int MOUSE_HIDE_DELAY = 3000;
  QTimer* m_mouse_timer;
  QPoint m_last_mouse{};
  bool m_cursor_locked = false;
  bool m_lock_cursor_on_next_activation = false;
  bool m_dont_lock_cursor_on_show = false;
  bool m_waiting_for_message_box = false;
  bool m_should_unpause_on_focus = false;
};
