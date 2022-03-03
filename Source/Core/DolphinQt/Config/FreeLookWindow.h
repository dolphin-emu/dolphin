// Copyright 2020 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;

class FreeLookWindow final : public QDialog
{
  Q_OBJECT
public:
  explicit FreeLookWindow(QWidget* parent);

private:
  void CreateMainLayout();

  QDialogButtonBox* m_button_box;
};
