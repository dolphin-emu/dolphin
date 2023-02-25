// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QComboBox>

class ToolTipComboBox : public ToolTipWidget<QComboBox>
{
private:
  QPoint GetToolTipPosition() const override;
};
