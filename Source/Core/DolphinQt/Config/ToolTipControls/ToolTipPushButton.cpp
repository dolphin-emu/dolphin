// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipPushButton.h"

ToolTipPushButton::ToolTipPushButton(const QString& text, QWidget* parent)
    : ToolTipWidget(text, parent)
{
}

QPoint ToolTipPushButton::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
