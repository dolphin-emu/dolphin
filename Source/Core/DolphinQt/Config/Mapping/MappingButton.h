// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include "DolphinQt/QtUtils/ElidedButton.h"

class ControlReference;
class MappingWidget;
class MappingWindow;
class QEvent;
class QMouseEvent;

class MappingButton : public ElidedButton
{
  Q_OBJECT
public:
  MappingButton(MappingWidget* widget, ControlReference* ref, bool indicator);

  bool IsInput() const;
  ControlReference* GetControlReference();
  void StartMapping();

signals:
  void ConfigChanged();

private:
  void Clear();
  void UpdateIndicator();
  void AdvancedPressed();

  void Clicked();
  void mouseReleaseEvent(QMouseEvent* event) override;

  MappingWindow* const m_mapping_window;
  ControlReference* const m_reference;
  bool m_is_mapping = false;
};
