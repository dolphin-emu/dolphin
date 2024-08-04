// Copyright 2023 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipWidget.h"

#include "DolphinQt/QtUtils/NonDefaultQPushButton.h"

class ToolTipPushButton : public ToolTipWidget<NonDefaultQPushButton>
{
public:
  explicit ToolTipPushButton(const QString& text = {}, QWidget* parent = nullptr);

private:
  QPoint GetToolTipPosition() const override;
};
