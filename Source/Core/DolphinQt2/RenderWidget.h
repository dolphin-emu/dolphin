// Copyright 2015 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <QEvent>
#include <QWidget>

class RenderWidget final : public QWidget
{
  W_OBJECT(RenderWidget)

public:
  explicit RenderWidget(QWidget* parent = nullptr);

  bool event(QEvent* event);

  void EscapePressed() W_SIGNAL(EscapePressed);
  void Closed() W_SIGNAL(Closed);
  void HandleChanged(void* handle) W_SIGNAL(HandleChanged, (void*), handle);
  void FocusChanged(bool focus) W_SIGNAL(FocusChanged, (bool), focus);
  void StateChanged(bool fullscreen) W_SIGNAL(StateChanged, (bool), fullscreen);

private:
  void OnHideCursorChanged();
};
