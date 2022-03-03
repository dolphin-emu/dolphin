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

  bool GetValue() const;

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  const TASInputWindow* m_parent;
  int m_frame_turbo_started = 0;
  int m_turbo_press_frames = 0;
  int m_turbo_total_frames = 0;
};
