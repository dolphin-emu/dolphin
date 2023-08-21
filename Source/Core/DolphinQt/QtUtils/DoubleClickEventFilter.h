// Copyright 2017 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QObject>

class DoubleClickEventFilter : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;

signals:
  void doubleClicked();

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
