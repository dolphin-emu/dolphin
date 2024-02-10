// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include <QWidget>

#include "DolphinQt/Settings.h"

#ifdef _WIN32
#include <dwmapi.h>
#endif

void SetQWidgetWindowDecorations(QWidget* widget)
{
#ifdef _WIN32
  if (!Settings::Instance().IsThemeDark())
    return;

  BOOL use_dark_title_bar = TRUE;
  DwmSetWindowAttribute(HWND(widget->winId()),
                        20 /* DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE */,
                        &use_dark_title_bar, DWORD(sizeof(use_dark_title_bar)));
#endif
}
