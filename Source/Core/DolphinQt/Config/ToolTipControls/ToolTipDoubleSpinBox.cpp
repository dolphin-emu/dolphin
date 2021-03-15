// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ToolTipControls/ToolTipDoubleSpinBox.h"

QPoint ToolTipDoubleSpinBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
