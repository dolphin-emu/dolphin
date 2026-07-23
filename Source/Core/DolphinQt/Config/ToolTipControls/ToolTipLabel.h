// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include <QLabel>

class ToolTipLabel : public ToolTipWidget<QLabel>
{
public:
  explicit ToolTipLabel(const QString& label);

private:
  QPoint GetToolTipPosition() const override;
};
