// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt/Config/ToolTipControls/ToolTipComboBox.h"

#include "Common/Config/Config.h"

class GraphicsChoice : public ToolTipComboBox
{
  Q_OBJECT
public:
  GraphicsChoice(const QStringList& options, const Config::Info<int>& setting);

private:
  void Update(int choice);

  Config::Info<int> m_setting;
};
