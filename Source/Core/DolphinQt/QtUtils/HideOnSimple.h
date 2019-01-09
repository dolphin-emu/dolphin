// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QList>
#include <QWidget>

#include "DolphinQt/Settings.h"

inline void HideOnSimple(QList<QWidget*> widgets)
{
  for (const auto& w : widgets)
  {
    QObject::connect(&Settings::Instance(), &Settings::SimplifiedModeToggled, w,
                     &QWidget::setHidden);

    w->setHidden(Settings::Instance().IsSimplifiedModeEnabled());
  }
}
