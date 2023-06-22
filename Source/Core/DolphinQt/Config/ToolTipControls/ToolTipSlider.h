// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QSlider>

class ToolTipSlider : public ToolTipWidget<QSlider>
{
public:
  explicit ToolTipSlider(Qt::Orientation orientation);

private:
  QPoint GetToolTipPosition() const override;
};
