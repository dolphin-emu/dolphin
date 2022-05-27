// Copyright 2022 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

NonDefaultQPushButton::NonDefaultQPushButton(const QString& text, QWidget* parent)
    : QPushButton(text, parent)
{
  this->setAutoDefault(false);
}
