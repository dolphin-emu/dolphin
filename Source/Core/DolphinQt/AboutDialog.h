// Copyright 2016 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class AboutDialog final : public QDialog
{
  Q_OBJECT
public:
  explicit AboutDialog(QWidget* parent = nullptr);
};
