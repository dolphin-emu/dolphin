// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/QtUtils/SetWindowDecorations.h"

#include <QWidget>

#include "DolphinQt/Settings.h"

#ifdef _WIN32
#include <Windows.h>

#include "Common/DynamicLibrary.h"

using PDwmSetWindowAttribute = HRESULT(__stdcall*)(HWND hwnd, DWORD dwAttribute,
                                                   LPCVOID pvAttribute, DWORD cbAttribute);
#endif

void SetQWidgetWindowDecorations(QWidget* widget)
{
#ifdef _WIN32
  if (!Settings::Instance().IsSystemDark())
    return;

  Common::DynamicLibrary dll("Dwmapi.dll");
  if (!dll.IsOpen())
    return;

  PDwmSetWindowAttribute func = nullptr;
  if (!dll.GetSymbol("DwmSetWindowAttribute", &func))
    return;

  BOOL use_dark_title_bar = TRUE;
  func(HWND(widget->winId()), 20 /* DWMWINDOWATTRIBUTE::DWMWA_USE_IMMERSIVE_DARK_MODE */,
       &use_dark_title_bar, DWORD(sizeof(use_dark_title_bar)));
#endif
}
