// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QRadioButton>

class ToolTipRadioButton : public ToolTipWidget<QRadioButton>
{
public:
  explicit ToolTipRadioButton(const QString& label);

private:
  QPoint GetToolTipPosition() const override;
};
