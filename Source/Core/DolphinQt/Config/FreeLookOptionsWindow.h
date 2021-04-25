// Copyright 2021 Dolphin Emulator Project
// SPDX-License-Identifier: GPL-2.0-or-later

#pragma once

#include <QDialog>

class QDialogButtonBox;

class FreeLookOptionsWindow final : public QDialog
{
  Q_OBJECT
public:
  FreeLookOptionsWindow(QWidget* parent, int index);

private:
  void CreateMainLayout();

  int m_port;
  QDialogButtonBox* m_button_box;
};
