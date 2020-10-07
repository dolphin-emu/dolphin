// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

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
  int m_frame_turbo_started;
  int m_turbo_press_frames;
  int m_turbo_total_frames;
};
