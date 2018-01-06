// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "DolphinQt2/QtUtils/ElidedButton.h"

class ControlReference;
class MappingWidget;
class QEvent;
class QMouseEvent;

class MappingButton : public ElidedButton
{
public:
  MappingButton(MappingWidget* widget, ControlReference* ref);

  void Clear();
  void Update();

private:
  bool event(QEvent* event) override;
  void mouseReleaseEvent(QMouseEvent* event) override;

  void OnButtonPressed();
  void OnButtonTimeout();
  void Connect();
  void SetBlockInputs(const bool block);

  MappingWidget* m_parent;
  ControlReference* m_reference;
  bool m_block = false;
};
