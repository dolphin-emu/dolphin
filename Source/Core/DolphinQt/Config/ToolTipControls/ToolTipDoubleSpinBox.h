// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QDoubleSpinBox>

class ToolTipDoubleSpinBox : public ToolTipWidget<QDoubleSpinBox>
{
private:
  QPoint GetToolTipPosition() const override;
};
