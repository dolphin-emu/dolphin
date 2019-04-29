// Copyright 2019 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QCheckBox>

class QMouseEvent;

class TASCheckBox : public QCheckBox
{
  Q_OBJECT
public:
  explicit TASCheckBox(const QString& text);

  bool GetValue();

protected:
  void mousePressEvent(QMouseEvent* event) override;

private:
  bool m_trigger_on_odd;
};
