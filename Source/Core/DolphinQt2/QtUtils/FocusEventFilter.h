// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class FocusEventFilter : public QObject
{
  Q_OBJECT
signals:
  void focusInEvent();
  void focusOutEvent();

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
