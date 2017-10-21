// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "Common/Flag.h"
#include "DolphinQt2/QtUtils/ElidedButton.h"

class MappingWidget;
class QEvent;
class QMouseEvent;

namespace ControllerEmu
{
class ControlReference;
};

class MappingButton : public ElidedButton
{
  Q_OBJECT
public:
  MappingButton(MappingWidget* widget, ControllerEmu::ControlReference* ref);

  void Clear();
  void Update();

signals:
  void AdvancedPressed();

private:
  void mouseReleaseEvent(QMouseEvent* event) override;

  void OnButtonPressed();
  void OnButtonTimeout();
  void Connect();

  MappingWidget* m_parent;
  ControllerEmu::ControlReference* m_reference;
};
