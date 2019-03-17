// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class DoubleClickEventFilter : public QObject
{
  Q_OBJECT
public:
  explicit DoubleClickEventFilter(QObject* parent);

signals:
  void doubleClicked();

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
