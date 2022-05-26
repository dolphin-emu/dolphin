// Copyright 2019 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QCheckBox>

class QMouseEvent;
class TASInputWindow;

class TASCheckBox : public QCheckBox
{
  Q_OBJECT
public:
  explicit TASCheckBox(const QString& text, TASInputWindow* parent);

  bool GetValue();
  bool GetIsTurbo();
  bool GetStateChanged();

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  void setCheckboxWeight(QFont::Weight weight);

  const TASInputWindow* m_parent;
  int m_frame_turbo_started = 0;
  int m_turbo_press_frames = 0;
  int m_turbo_total_frames = 0;
  bool m_is_turbo = false;
  bool m_state_changed = false;
};
