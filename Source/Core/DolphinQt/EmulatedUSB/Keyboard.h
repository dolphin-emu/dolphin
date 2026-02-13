// Copyright 2026 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QWidget>

class KeyboardWindow : public QWidget
{
  Q_OBJECT
public:
  explicit KeyboardWindow(QWidget* parent = nullptr);
  ~KeyboardWindow() override;
};
