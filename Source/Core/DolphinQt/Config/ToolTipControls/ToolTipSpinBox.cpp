// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipSpinBox.h"

QPoint ToolTipSpinBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
