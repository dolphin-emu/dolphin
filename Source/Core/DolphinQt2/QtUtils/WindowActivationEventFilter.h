// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <QObject>

class WindowActivationEventFilter : public QObject
{
  W_OBJECT(WindowActivationEventFilter)

public:
  void windowActivated() W_SIGNAL(windowActivated);
  void windowDeactivated() W_SIGNAL(windowDeactivated);

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
