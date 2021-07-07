// Copyright 2021 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <QDialog>

class QDialogButtonBox;

class FreeLook2DOptionsWindow final : public QDialog
{
  Q_OBJECT
public:
  FreeLook2DOptionsWindow(QWidget* parent, int index);

private:
  void CreateMainLayout();

  int m_port;
  QDialogButtonBox* m_button_box;
};
