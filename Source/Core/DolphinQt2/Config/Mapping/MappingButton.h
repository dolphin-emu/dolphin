// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include "Common/Flag.h"
#include "DolphinQt2/QtUtils/ElidedButton.h"

class ControlReference;
class MappingWidget;
class QEvent;
class QMouseEvent;

class MappingButton : public ElidedButton
{
  W_OBJECT(MappingButton)
public:
  MappingButton(MappingWidget* widget, ControlReference* ref);

  void Clear();
  void Update();

  void AdvancedPressed() W_SIGNAL(AdvancedPressed);

private:
  bool event(QEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  void OnButtonPressed();
  void OnButtonTimeout();
  void Connect();

  MappingWidget* m_parent;
  ControlReference* m_reference;
  Common::Flag m_block;
};
