// Copyright 2020 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>
#include <QWidget>

class NKitWarningDialog final : public QDialog
{
  Q_OBJECT

public:
  static bool ShowUnlessDisabled(QWidget* parent = nullptr);

private:
  explicit NKitWarningDialog(QWidget* parent = nullptr);
};
