// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "Common/Flag.h"
#include "DolphinQt/QtUtils/ElidedButton.h"

class ControlReference;
class MappingWidget;
class QEvent;
class QMouseEvent;

class MappingButton : public ElidedButton
{
  Q_OBJECT
public:
  MappingButton(MappingWidget* widget, ControlReference* ref, bool indicator);

  bool IsInput() const;

private:
  void Clear();
  void UpdateIndicator();
  void ConfigChanged();
  void AdvancedPressed();

  void Clicked();
  void mouseReleaseEvent(QMouseEvent* event) override;

  MappingWidget* m_parent;
  ControlReference* m_reference;
};
