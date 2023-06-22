// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QTabWidget>

class QEvent;

class PartiallyClosableTabWidget : public QTabWidget
{
  Q_OBJECT
public:
  PartiallyClosableTabWidget(QWidget* parent = nullptr);

  void setTabUnclosable(int index);
};
