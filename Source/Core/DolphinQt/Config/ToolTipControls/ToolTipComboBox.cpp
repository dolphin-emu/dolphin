// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

QPoint ToolTipComboBox::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
