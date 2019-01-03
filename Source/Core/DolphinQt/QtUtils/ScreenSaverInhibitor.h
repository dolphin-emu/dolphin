// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

#if defined(__APPLE__)
#include <IOKit/pwr_mgt/IOPMLib.h>
#elif !defined(_WIN32)
#include <QMainWindow>
#endif

class ScreenSaverInhibitor : public QObject
{
  Q_OBJECT

public:
  explicit ScreenSaverInhibitor(QObject* parent);
  ~ScreenSaverInhibitor() override;

private:
#if defined(__APPLE__)
  IOPMAssertionID m_power_assertion = kIOPMNullAssertionID;
#elif !defined(_WIN32)
  uint m_cookie = UINT_MAX;  // For DBus inhibit
  WId m_window_id = 0;       // For xdg-screensaver fallback inhibit
#endif
};
