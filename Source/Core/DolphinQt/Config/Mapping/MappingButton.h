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
  enum class ControlType
  {
    NormalInput,
    ModifierInput,
    Output,
  };

  MappingButton(MappingWidget* widget, ControlReference* ref, ControlType type);

  ControlReference* GetControlReference();
  ControlType GetControlType() const;

signals:
  void ConfigChanged();
  void QueueNextButtonMapping();

private:
  void Clear();
  void AdvancedPressed();

  void Clicked();
  void mouseReleaseEvent(QMouseEvent* event) override;

  MappingWindow* const m_mapping_window;
  ControlReference* const m_reference;
  const ControlType m_control_type;
};
