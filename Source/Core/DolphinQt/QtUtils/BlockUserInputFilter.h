// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QObject>

class QEvent;

class BlockUserInputFilter : public QObject
{
  Q_OBJECT
public:
  using QObject::QObject;

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
