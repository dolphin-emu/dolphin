// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QCheckBox>

class ToolTipCheckBox : public ToolTipWidget<QCheckBox>
{
public:
  explicit ToolTipCheckBox(const QString& label);

private:
  QPoint GetToolTipPosition() const override;
};
