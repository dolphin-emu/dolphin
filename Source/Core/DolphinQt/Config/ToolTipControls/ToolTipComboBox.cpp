// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

QPoint ToolTipComboBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
