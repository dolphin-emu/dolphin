// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ToolTipControls/ToolTipSpinBox.h"

QPoint ToolTipSpinBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
