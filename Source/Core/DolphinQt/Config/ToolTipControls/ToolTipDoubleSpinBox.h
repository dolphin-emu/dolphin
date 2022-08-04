// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QDoubleSpinBox>

class ToolTipDoubleSpinBox : public ToolTipWidget<QDoubleSpinBox>
{
private:
  QPoint GetToolTipPosition() const override;
};
