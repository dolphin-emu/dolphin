// Copyright 2017 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <wobjectdefs.h>
#include <QObject>

class DoubleClickEventFilter : public QObject
{
  W_OBJECT(DoubleClickEventFilter)

public:
  void doubleClicked() W_SIGNAL(doubleClicked);

private:
  bool eventFilter(QObject* object, QEvent* event) override;
};
