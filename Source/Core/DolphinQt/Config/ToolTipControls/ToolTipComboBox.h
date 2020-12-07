// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QComboBox>

class ToolTipComboBox : public ToolTipWidget<QComboBox>
{
private:
  QPoint GetToolTipPosition() const override;
};
