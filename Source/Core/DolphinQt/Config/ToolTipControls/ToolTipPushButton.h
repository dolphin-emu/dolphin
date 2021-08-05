// Copyright 2022 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QPushButton>

class ToolTipPushButton : public ToolTipWidget<QPushButton>
{
public:
  using ToolTipWidget<QPushButton>::ToolTipWidget;

private:
  QPoint GetToolTipPosition() const override;
};
