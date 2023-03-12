// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCheckBox>

#include "DolphinQt/TAS/TASControlState.h"

class QMouseEvent;
class TASInputWindow;

class TASCheckBox : public QCheckBox
{
  Q_OBJECT
public:
  explicit TASCheckBox(const QString& text, TASInputWindow* parent);

  // Can be called from the CPU thread
  bool GetValue() const;
  // Must be called from the CPU thread
  void OnControllerValueChanged(bool new_value);

protected:
  void mousePressEvent(QMouseEvent* event) override;

private slots:
  void OnUIValueChanged(int new_value);
  void ApplyControllerValueChange();

private:
  const TASInputWindow* m_parent;
  TASControlState m_state;
  int m_frame_turbo_started = 0;
  int m_turbo_press_frames = 0;
  int m_turbo_total_frames = 0;
};
