// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipDoubleSpinBox.h"

QPoint ToolTipDoubleSpinBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
