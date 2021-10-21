// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QSpinBox>

class ToolTipSpinBox : public ToolTipWidget<QSpinBox>
{
private:
  QPoint GetToolTipPosition() const override;
};
