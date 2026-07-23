// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#include "DolphinQt/Config/ToolTipControls/ToolTipLabel.h"

ToolTipLabel::ToolTipLabel(const QString& label) : ToolTipWidget(label)
{
  SetTitle(label);
}

QPoint ToolTipLabel::GetToolTipPosition() const
{
  return pos() + QPoint(width() / 2, height() / 2);
}
